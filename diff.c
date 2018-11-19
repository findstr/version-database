#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "dr.h"
#include "dir.h"
#include "db.h"
#include "dc3.h"
#include "object.h"
#include "meta.h"
#include "diff.h"

#define COST		(10)
#define MIN(a, b)	((a) > (b) ? (b) : (a))
#define ENABLE_DEL	(1)
/*
static void
print(const uint8_t *from, const uint8_t *to)
{
	while (from < to)
		fprintf(stderr, "%c", *from++);
	printf("\n");
}*/

static int
match(const uint8_t *a, int an, const uint8_t *b, int bn, int *n)
{
	int i, x = MIN(an, bn);
	for (i = 0; i < x; i++) {
		if (a[i] != b[i])
			break;
	}
	*n = i;
	if (i < x) {
		if (a[i] < b[i]) {
			return -1;
		} else if (a[i] > b[i]) {
			return 1;
		} else {
			return 0;
		}
	}
	if (x == bn)
		return 0;
	return -1;
}

static int
search(dr_t old, int *SA, const uint8_t *ns, int nsize, int *pos)
{
	int n = 0, sim = 0;
	int osize = old->size;
	int m = 0, l = 0, r = osize;
	const uint8_t *p = old->buf;
	*pos = -1;
	while (l < r) {
		int c, s;
		m = (l + r) / 2;
		n = SA[m];
		c = match(&p[n], osize - n, ns, nsize, &s);
		if (c == 0) {	//contain
			*pos = n;
			sim = s;
			assert(s == nsize);
			break;
		} else {
			if (sim < s) {
				*pos = n;
				sim = s;
			}
			if (c < 0)
				l = m + 1;
			else
				r = m;
		}
	}
	return sim;
}

static dr_t
diff_content(dr_t old, dr_t new)
{
	int *SA;
	drb_t drb;
	const uint8_t *np;
	int ni = 0, oi = 0, sim;
	int nsize = new->size;
	np = new->buf;
	SA = dc3(old->buf, old->size);
	drb_init(&drb, 1024);
	patch_total(&drb, new->size);
	while (ni < nsize) {
		struct COPY copy;
		sim = search(old, SA, &np[ni], nsize - ni, &oi);
		assert(sim <= nsize - ni);
		if (sim <= COST) { //has no match, so find first match segment
			struct INSERT insert;
			int xi, xn = 0, xs = 0;
			for (xi = ni+1; xi < nsize; xi++) {
				xs = search(old, SA, &np[xi], nsize - xi, &xn);
				assert(xs <= (nsize - xi));
				if (xs > COST)
					break;
			}
			insert.p = &new->buf[ni];
			insert.size = xi - ni;
			patch_insert(&drb, &insert);
			printf("INSERTT INTO %d SIZE %d\n", ni, xi - ni);
			ni = xi;
			oi = xn;
			sim = xs;
		}
		if (sim > COST) {
			copy.pos = oi;
			copy.size = sim;
			patch_copy(&drb, &copy);
			printf("COPY FROM %d TO %d SIZE %d\n", oi, ni, sim);
			ni += sim;
		}
	}
	free(SA);
	//printf("drb size:%d\n", drb.size);
	return drb_dump(&drb);
}

void
diff(struct diff_args *args)
{
	int i, j;
	drb_t file;
	dr_t data;
	dr_t patchhash;
	char buff[2048];
	struct ref *r;
	struct release ra, rb;
	struct tree ta, tb;
	int add, dff, mov, del;
	db_readrel(&ra, args->a);
	db_readrel(&rb, args->b);
	db_readtree(&ta, ra.tree);
	db_readtree(&tb, rb.tree);
	printf("tree a:%s\n", ra.tree->buf);
	printf("tree b:%s\n", rb.tree->buf);
	release_destroy(&ra);
	release_destroy(&rb);
	tree_sort(&ta, ref_hashcmp);
	drb_init(&file, 1024);
	printf("diff start\n");
	printf("detect change\n");
	add = 0; dff = 0; mov = 0; del = 0;
	for (i = 0; i < tb.refn; i++) {
		int acost = 0;
		struct ref *b;
		struct ref *a, *x;
		dr_t patch = NULL;
		b = &tb.refs[i];
		if ((x = tree_search(&ta, b, ref_hashcmp)) != NULL) {
			if (dr_cmp(b->name, x->name) != 0) {
				struct MOV M;
				M.namea = x->name;
				M.name = b->name;
				ctrl_mov(&file, &M);
				mov++;
			} else {
				//printf("%s is same, skip it.\n", b->name->buf);
			}
			continue;
		}
		if (b->data == NULL)
			b->data = db_read(b->hash);
		for (j = 0; j < ta.refn; j++) {
			int cost;
			dr_t xp;
			struct ref *x;
			x = &ta.refs[j];
			if (x->data == NULL)
				x->data = db_read(x->hash);
			if (dr_cmp(b->name, x->name) == 0)
				cost = 0;
			else
				cost = x->name->size + 4;
			xp = diff_content(x->data, b->data);
			if (patch == NULL ||
				(xp->size + cost) < (patch->size + acost)) {
				patch = dr_ref(xp);
				a = x;
				acost = cost;
			}
			dr_unref(xp);
		}
		if (patch == NULL || b->data->size <= patch->size) {//no fix patch
			struct NEW N;
			N.name = b->name;
			N.data = b->data;
			ctrl_new(&file, &N);
			++add;
		} else if (acost == 0) {	//great! same name
			struct DFF D;
			assert(dr_cmp(b->name, a->name) == 0);
			D.name = b->name;
			D.patch = patch;
			ctrl_dff(&file, &D);
			++dff;
		} else {
			struct DFX D;
			D.namea = a->name;
			D.name = b->name;
			D.patch = patch;
			ctrl_dfx(&file, &D);
			++dff;
		}
		dr_unref(patch);
	}
	printf("\ndetect remove\n");
	tree_sort(&tb, ref_namecmp);
#if ENABLE_DEL
	for (i = 0; i < ta.refn; i++) {
		r = &ta.refs[i];
		if (tree_search(&tb, r, ref_namecmp) == NULL) { //clear
			struct DEL d;
			d.name = r->name;
			ctrl_del(&file, &d);
			++del;
		}
	}
#endif
	printf("\n");
	data = drb_dump(&file);
	patchhash = db_hash(data);
	dir_writefile("patch.dat", data, args->p);
	dr_unref(data);
	snprintf(buff, sizeof(buff),
		"{\"add\":%d, \"dff\":%d, \"mov\":%d, \"del\":%d, \"hash\":\"%s\"}",
		add, dff, mov, del, patchhash->buf);
	data = dr_newstr(buff);
	dir_writefile("patch.json", data, args->p);
	dr_unref(data);
	dr_unref(patchhash);
	tree_destroy(&ta);
	tree_destroy(&tb);
	return ;
}


#ifdef MAIN

int main(int argc, const char *argv[])
{
	dr_t src, dst, patch;
	if (argc != 3) {
		fprintf(stderr, "USAGE: %s source destination\n", argv[0]);
		exit(0);
	}
	src = dir_readfile(argv[1], NULL);
	if (src == NULL) {
		fprintf(stderr, "ERROR: file %s nonexist\n", argv[1]);
		exit(errno);
	}
	dst = dir_readfile(argv[2], NULL);
	if (dst == NULL) {
		fprintf(stderr, "ERROR: file %s nonexist\n", argv[2]);
		exit(errno);
	}
	patch = diff_content(src, dst);
	dir_writefile("x.patch", patch, NULL);
	dr_unref(src);
	dr_unref(dst);
	dr_unref(patch);
	return 0;
}

#endif


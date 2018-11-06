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
#include "diff.h"

#define COST		(10)
#define MIN(a, b)	((a) > (b) ? (b) : (a))
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

static void
TARGET_SIZE(drb_t *drb, int size)
{
	uint8_t *p = drb_check(drb, 4);
	uint32(p) = size;	p += 4;
	drb_end(drb, p);
}

static void
INSERT_INTO(drb_t *drb, const uint8_t *np, int size)
{
	uint8_t *p = drb_check(drb, 1+4+size);
	uint8(p) = INSERT;	p += 1;
	uint32(p) = size;	p += 4;
	memcpy(p, np, size);	p += size;
	drb_end(drb, p);
	return ;
}

static void
COPY_FROM(drb_t *drb, uint32_t pos, uint32_t size)
{
	uint8_t *p = drb_check(drb, 1+4+4);
	uint8(p) = COPY;	p += 1;
	uint32(p) = pos;	p += 4;
	uint32(p) = size;	p += 4;
	drb_end(drb, p);
	return ;
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
	TARGET_SIZE(&drb, new->size);
	while (ni < nsize) {
		sim = search(old, SA, &np[ni], nsize - ni, &oi);
		if (sim < COST) { //has no match, so find first match segment
			int xi, xn, xs;
			for (xi = ni+1; xi < nsize; xi++) {
				xs = search(old, SA, &np[xi], nsize - xi, &xn);
				if (xs >= COST)
					break;
			}
			//printf("INSERTT INTO %d SIZE %d\n", ni, xi - ni);
			INSERT_INTO(&drb, &new->buf[ni], xi - ni);
			ni = xi;
			oi = xn;
			sim = xs;
		}
		//printf("COPY FROM %d TO %d SIZE %d\n", oi, ni, sim);
		COPY_FROM(&drb, oi, sim);
		ni += sim;
	}
	free(SA);
	//printf("drb size:%d\n", drb.size);
	return drb_dump(&drb);
}

void
diff(struct diff_args *args)
{
	int i, j;
	dr_t data;
	drb_t file;
	struct release ra, rb;
	struct tree ta, tb;
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
	for (i = 0; i < tb.refn; i++) {
		int acost = 0;
		struct ref *b;
		struct ref *a, *x;
		dr_t patch = NULL;
		b = &tb.refs[i];
		if ((x = tree_search(&ta, b, ref_hashcmp)) != NULL) {
			if (dr_cmp(b->name, x->name) != 0) {
				struct MOV M;
				M.hash = b->hash;
				M.namea = x->name;
				M.name = b->name;
				ctrl_mov(&file, &M);
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
			N.name = (b->name);
			N.hash = (b->hash);
			N.data = (b->data);
			ctrl_new(&file, &N);
		} else if (acost == 0) {	//great! same name
			struct DFF D;
			assert(dr_cmp(b->name, a->name) == 0);
			D.name = b->name;
			D.hash = b->hash;
			D.patch = patch;
			ctrl_dff(&file, &D);
		} else {
			struct DFX D;
			D.namea = a->name;
			D.name = b->name;
			D.hash = b->hash;
			D.patch = patch;
			ctrl_dfx(&file, &D);
		}
		dr_unref(patch);
	}
	printf("\ndetect remove\n");
	tree_sort(&tb, ref_namecmp);
	for (i = 0; i < ta.refn; i++) {
		struct ref *r;
		r = &ta.refs[i];
		if (tree_search(&tb, r, ref_namecmp) == NULL) { //clear
			struct DEL d;
			d.name = r->name;
			ctrl_del(&file, &d);
		}
	}
	printf("\n");
	data = drb_dump(&file);
	dir_writefile((char *)args->p->buf, data, NULL);
	dr_unref(data);
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


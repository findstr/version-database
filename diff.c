#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "conf.h"
#include "dr.h"
#include "dir.h"
#include "db.h"
#include "dc3.h"
#include "object.h"
#include "meta.h"
#include "checkout.h"
#include "patch.h"
#include "diff.h"

#define COST		(10)
#define MIN(a, b)	((a) > (b) ? (b) : (a))

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
			//printf("INSERTT INTO %d SIZE %d\n", ni, xi - ni);
			ni = xi;
			oi = xn;
			sim = xs;
		}
		if (sim > COST) {
			copy.pos = oi;
			copy.size = sim;
			patch_copy(&drb, &copy);
			//printf("COPY FROM %d TO %d SIZE %d\n", oi, ni, sim);
			ni += sim;
		}
	}
	free(SA);
	//printf("drb size:%d\n", drb.size);
	return drb_dump(&drb);
}

static void
verify(dr_t a, dr_t b, dr_t p)
{
	char *s, *e, buf[1024];
	dr_t apath, bpath;
	dr_t fingera, fingerb;
	struct patch_args args;
	snprintf(buf, sizeof(buf), "_temp/%s/", str(a));
	apath = dr_newstr(buf);
	snprintf(buf, sizeof(buf), "_temp/%s/", str(b));
	bpath = dr_newstr(buf);
	//checkout
	checkout(a, apath);
	checkout(b, bpath);
	args.hash = a;
	args.patch = p;
	args.output = apath;
	patch(&args);
	//verify finger
	snprintf(buf, sizeof(buf), "%s/"FINGERPRINT_NAME, str(apath));
	fingera = dir_readfile(buf, NULL);
	snprintf(buf, sizeof(buf), "%s/"FINGERPRINT_NAME, str(bpath));
	fingerb = dir_readfile(buf, NULL);
	assert(dr_cmp(fingera, fingerb) == 0);
	//verify file
	s = e = str(fingera);
	e += fingera->size;
	while (s < e) {
		dr_t data, dhash;
		char *hash, *path;
		hash = s; s = strchr(s, '\t'); *s = 0;
		path = ++s; s = strchr(s, '\n'); *s++ = 0;
		data = dir_readfile(path, apath);
		dhash = db_hash(data);
		if (strcmp(str(dhash), hash) != 0) {
			fprintf(stderr, "%s != %s", hash, str(dhash));
			exit(EINVAL);
		}
		dr_unref(data);
		dr_unref(dhash);
	}
	dr_unref(fingera);
	dr_unref(fingerb);
	dr_unref(apath);
	dr_unref(bpath);
	system("rm -rf _temp");
}

static void
diff_tree(dr_t a, dr_t b, dr_t root)
{
	int i;
	drb_t file;
	dr_t data, afinger;
	dr_t patchhash;
	char buf[2048];
	struct ref *r;
	struct release ra, rb;
	struct tree tb, ta_name, ta_hash;
	int add, dff, cpy, del;
	a = db_aliashash(a);
	b = db_aliashash(b);
	db_readrel(&ra, a);
	db_readrel(&rb, b);
	db_readtree(&ta_name, ra.tree);
	db_readtree(&tb, rb.tree);
	afinger = dr_ref(ra.finger);
	printf("tree a:%s\n", ra.tree->buf);
	printf("tree b:%s\n", rb.tree->buf);
	release_destroy(&ra);
	release_destroy(&rb);
	tree_clone(&ta_hash, &ta_name);
	tree_sort(&ta_name, ref_namecmp);
	tree_sort(&ta_hash, ref_hashcmp);
	drb_init(&file, 1024);
	printf("diff start\n");
	printf("detect change\n");
	add = 0; dff = 0; cpy = 0; del = 0;
	for (i = 0; i < tb.refn; i++) {
		struct ref *a, *b;
		dr_t patch = NULL;
		b = &tb.refs[i];
		if ((a = tree_search(&ta_hash, b, ref_hashcmp))) {
			if (dr_cmp(b->name, a->name) != 0) {
				struct CPY C;
				C.namea = a->name;
				C.name = b->name;
				ctrl_cpy(&file, &C);
				++cpy;
			} else {
				//printf("%s is same, skip it.\n", b->name->buf);
			}
			continue;
		}
		if (b->data == NULL)
			b->data = db_read(b->hash);
		a = tree_search(&ta_name, b, ref_namecmp);
		if (a == NULL) {
			struct NEW N;
			N.name = (b->name);
			N.data = (b->data);
			ctrl_new(&file, &N);
			add++;
		} else {
			if (a->data == NULL)
				a->data = db_read(a->hash);
			patch = diff_content(a->data, b->data);
			if (b->data->size <= patch->size) {//no fix patch
				struct NEW N;
				N.name = (b->name);
				N.data = (b->data);
				ctrl_new(&file, &N);
				add++;
			} else {
				struct DFF D;
				D.name = b->name;
				D.patch = patch;
				ctrl_dff(&file, &D);
				++dff;
			}
			dr_unref(patch);
		}
	}
	printf("detect recpye\n");
#if ENABLE_DEL
	tree_sort(&tb, ref_namecmp);
	for (i = 0; i < ta_name.refn; i++) {
		r = &ta_name.refs[i];
		if (tree_search(&tb, r, ref_namecmp) == NULL) { //clear
			struct DEL d;
			d.name = r->name;
			ctrl_del(&file, &d);
			++del;
		}
	}
#endif
	data = drb_dump(&file);
	patchhash = db_hash(data);
	snprintf(buf, sizeof(buf), "%s/patch.dat", str(afinger));
	dir_writefile(buf, data, root);
	snprintf(buf, sizeof(buf),
		"{\"add\":%d, \"dff\":%d, \"cpy\":%d, "
		"\"del\":%d, \"hash\":\"%s\", \"size\":%d }",
		add, dff, cpy, del, patchhash->buf, data->size);
	dr_unref(data);
	data = dr_newstr(buf);
	snprintf(buf, sizeof(buf), "%s/patch.json", str(afinger));
	dir_writefile(buf, data, root);
	printf("%s\n\n", str(data));
	snprintf(buf, sizeof(buf), "%s/%s/patch.dat", str(root), str(afinger));
	dr_unref(data);
	dr_unref(patchhash);
	dr_unref(afinger);
	tree_destroy(&ta_name);
	tree_destroy(&ta_hash);
	tree_destroy(&tb);
	data = dr_newstr(buf);
	verify(a, b, data);
	dr_unref(a);
	dr_unref(b);
	dr_unref(data);
	return ;
}

#if 1
void
diff(struct diff_args *args)
{
	dr_t head;
	struct release rel;
	if (args->r == 0)
		return diff_tree(args->a, args->b, args->o);
	head = db_readhead(&rel, NULL);
	if (head->size == 0) {
		printf("Empty db\n");
	} else {
		int i;
		for (i = 0; i < args->r || args->r < 0; i++) {
			dr_t h;
			if (rel.prev->size <= 0)
				break;
			h = dr_ref(rel.prev);
			diff_tree(h, head, args->o);
			release_destroy(&rel);
			db_readrel(&rel, h);
			dr_unref(h);
		}
	}
	dr_unref(head);
	release_destroy(&rel);
	return ;

}

#else
static int
cmpstr(dr_t a, dr_t b)
{
	char c1, c2, c;
	const char *sa, *ea, *sb, *eb;
	sa = (char *)a->buf;
	sb = (char *)b->buf;
	ea = sa + a->size - 13;
	eb = sb + b->size - 13;
	while (sa < ea && sb < eb) {
		c1 = *sa++;
		c2 = *sb++;
		c = c1 - c2;
		if (c != 0)
			return c;
	}
	return (ea - sa) - (eb - sb);
}

static int
shortnamecmp(const void *aa, const void *bb)
{
	struct ref *a = (struct ref *)aa;
	struct ref *b = (struct ref *)bb;
	return cmpstr(a->name, b->name);
}

void
diff(struct diff_args *args)
{
	int i;
	drb_t file;
	char buff[2048];
	dr_t data, patchhash;
	int add, dff, cpy, del;
	struct release ra, rb;
	struct tree ta, tb;
	db_aliashash(&args->a);
	db_aliashash(&args->b);
	db_readrel(&ra, args->a);
	db_readrel(&rb, args->b);
	db_readtree(&ta, ra.tree);
	db_readtree(&tb, rb.tree);
	printf("tree a:%s\n", ra.tree->buf);
	printf("tree b:%s\n", rb.tree->buf);
	release_destroy(&ra);
	release_destroy(&rb);
	tree_sort(&ta, shortnamecmp);
	drb_init(&file, 1024);
	printf("diff start\n");
	printf("detect change\n");
	add = 0; dff = 0; cpy = 0; del = 0;
	for (i = 0; i < tb.refn; i++) {
		struct ref *b;
		struct ref *a;
		dr_t patch = NULL;
		b = &tb.refs[i];
		if (b->data == NULL)
			b->data = db_read(b->hash);
		a = tree_search(&ta, b, shortnamecmp);
		if (a == NULL) {
			struct NEW N;
			N.name = (b->name);
			N.data = (b->data);
			ctrl_new(&file, &N);
			add++;
			continue;
		}
		if (dr_cmp(a->hash, b->hash) == 0) {
			if (dr_cmp(b->name, a->name) != 0) {
				struct MOV M;
				M.namea = a->name;
				M.name = b->name;
				ctrl_cpy(&file, &M);
				++cpy;
			} else {
				//printf("%s is same, skip it.\n", b->name->buf);
			}
			continue;
		}
		if (a->data == NULL)
			a->data = db_read(a->hash);
		patch = diff_content(a->data, b->data);
		if (b->data->size <= patch->size) {//no fix patch
			struct NEW N;
			N.name = (b->name);
			N.data = (b->data);
			ctrl_new(&file, &N);
			add++;
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
	printf("\ndetect recpye\n");
	tree_sort(&tb, ref_namecmp);
#if ENABLE_DEL
	for (i = 0; i < ta.refn; i++) {
		struct ref *r = &ta.refs[i];
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
		"{\"add\":%d, \"dff\":%d, \"cpy\":%d, \"del\":%d, \"hash\":\"%s\"}",
		add, dff, cpy, del, patchhash->buf);
	data = dr_newstr(buff);
	dir_writefile("patch.json", data, args->p);
	dr_unref(data);
	dr_unref(patchhash);
	tree_destroy(&ta);
	tree_destroy(&tb);
	return ;
}

#endif


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


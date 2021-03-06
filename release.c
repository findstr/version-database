#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "conf.h"
#include "db.h"
#include "dir.h"
#include "db.h"
#include "time.h"
#include "object.h"
#include "checkout.h"
#include "release.h"

static dr_t
fingerprint(struct tree *tree)
{
	int i, n;
	drb_t drb;
	dr_t finger;
	struct ref *r;
	char buf[2048];
	drb_init(&drb, 2048);
	for (i = 0; i < tree->refn; i++) {
		uint8_t *p;
		r = &tree->refs[i];
		n = snprintf(buf, sizeof(buf), "%s\t%s\n",
			r->hash->buf, r->name->buf);
		assert(n < sizeof(buf));
		p = drb_check(&drb, n);
		memcpy(p, buf, n); p += n;
		drb_end(&drb, p);
	}
	finger = drb_dump(&drb);
	r = &tree->refs[i];
	r->name = dr_newstr(FINGERPRINT_NAME);
	r->hash = db_write(r->name, finger);
	++tree->refn;
	dr_unref(finger);
	return dr_ref(r->hash);
}

void
release(struct release_args *args)
{
	dr_t *list, head;
	int i, n, keep, del;
	struct release prevrel;
	struct tree prevtree, tree;
	n = dir_scan(str(args->fromdir), &list, 1);
	head = db_readhead(&prevrel, &prevtree);
	tree_sort(&prevtree, ref_namecmp);
	tree.refn = n;
	tree.refs = malloc(sizeof(struct ref) * (n+1));
	memset(tree.refs, 0, sizeof(struct ref) * (n+1));
	keep = 0;
	for (i = 0; i < n; i++) {
		dr_t d, name;
		struct ref *r, *f;
		r = &tree.refs[i];
		r->name = name = list[i];
		d = dir_readfile(str(name), args->fromdir);
		r->hash = db_write(name, d);
		dr_unref(d);
		f = tree_search(&prevtree, r, ref_namecmp);
		if (f && dr_cmp(f->hash, r->hash) == 0) {
			dr_unref(f->hash);
			f->hash= NULL;
			keep += 1;
		}
	}
	del = 0;
	if (keep == n) {
		int x = prevtree.refn;
		for (i = 0; i < x; i++) {
			struct ref *r = &prevtree.refs[i];
			dr_t hash = r->hash;
			if (hash== NULL)
				continue;
			if (strcmp(str(r->name), FINGERPRINT_NAME) != 0)
				++del;
		}
		if (del == 0)
			printf("clean workspace\n");
	}
	if (keep != n || del != 0) {
		struct release rel;
		dr_t treeobj;//, finger;
		dr_t relhash;
		dr_t treehash;
		rel.time = time(NULL);
		rel.prev = dr_ref(head);
		rel.ver = dr_ref(args->version);
		rel.note = dr_ref(args->describe);
		tree_sort(&tree, ref_hashcmp);
#if ENABLE_FINGERPRINT
		rel.finger = fingerprint(&tree);
#endif
		treeobj = tree_marshal(&tree);
		treehash = db_write(rel.ver, treeobj);
		rel.tree = dr_ref(treehash);
		relhash = db_writehead(&rel);
		release_destroy(&rel);
		if (args->checkout != NULL)
			checkout(relhash, args->checkout);
		dr_unref(treeobj);
		dr_unref(relhash);
		dr_unref(treehash);
		printf("release total:%d changed:%d del:%d\n", n, n-keep, del);
	}
	free(list);
	dr_unref(head);
	release_destroy(&prevrel);
	tree_destroy(&prevtree);
	tree_destroy(&tree);
	return ;
}



#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "db.h"
#include "dir.h"
#include "db.h"
#include "time.h"
#include "object.h"
#include "release.h"

void
release(struct release_args *args)
{
	dr_t *list, head;
	int i, n, keep;
	struct release prevrel;
	struct tree prevtree, tree;
	n = dir_scan(str(args->fromdir), &list, 1);
	head = db_readhead(&prevrel, &prevtree);
	tree_sort(&prevtree, ref_hashcmp);
	tree.refn = n;
	tree.refs = malloc(sizeof(struct ref) * n);
	memset(tree.refs, 0, sizeof(struct ref) * n);
	keep = 0;
	for (i = 0; i < n; i++) {
		dr_t d, name;
		struct ref *r, *f;
		r = &tree.refs[i];
		r->name = name = list[i];
		d = dir_readfile(str(name), args->fromdir);
		r->hash = db_write(name, d);
		dr_unref(d);
		f = tree_search(&prevtree, r, ref_hashcmp);
		if (f && (strcmp(str(f->name), str(name)) == 0)) {
			dr_unref(f->name);
			f->name = NULL;
			keep += 1;
		}
	}
	if (keep == n) {
		int x = prevtree.refn;
		for (i = 0; i < x; i++) {
			if (prevtree.refs[i].name != NULL)
				break;
		}
		if (i >= x)
			printf("Clean workspace\n");
		else
			keep = 0;
	}
	if (keep != n) {
		struct release rel;
		dr_t treeobj;
		dr_t relhash, treehash;
		rel.time = time(NULL);
		rel.prev = dr_ref(head);
		rel.ver = dr_ref(args->version);
		rel.note = dr_ref(args->describe);
		treeobj = tree_marshal(&tree);
		treehash = db_write(rel.ver, treeobj);
		rel.tree = dr_ref(treehash);
		relhash = db_writehead(&rel);
		release_destroy(&rel);
		dr_unref(treeobj);
		dr_unref(treehash);
		dr_unref(relhash);
	}
	free(list);
	dr_unref(head);
	release_destroy(&prevrel);
	tree_destroy(&prevtree);
	tree_destroy(&tree);
	return ;
}



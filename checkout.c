#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "dr.h"
#include "dir.h"
#include "db.h"
#include "checkout.h"

void
checkout(dr_t hash, dr_t outdir)
{
	int i;
	struct release r;
	struct tree t;
	hash = db_aliashash(hash);
	db_readrel(&r, hash);
	dr_unref(hash);
	db_readtree(&t, r.tree);
	printf("checkout tree:%s->%s\n", r.tree->buf, str(outdir));
	release_destroy(&r);
	dir_ensure(str(outdir));
	if (errno == EEXIST)
		printf("warning: checkout folder '%s' is exist\n", str(outdir));
	for (i = 0; i < t.refn; i++) {
		struct ref *f;
		f = &t.refs[i];
		f->data = db_read(f->hash);
		dir_writefile(str(f->name), f->data, outdir);
	}
	tree_destroy(&t);
	return ;
}

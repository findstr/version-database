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
	int i, ret;
	struct release r;
	struct tree t;
	db_readrel(&r, hash);
	db_readtree(&t, r.tree);
	printf("tree:%s\n", r.tree->buf);
	release_destroy(&r);
	ret = mkdir(str(outdir), 0755);
	if (ret == -1) {
		fprintf(stderr, "checkout folder '%s' is exist\n",
			outdir->buf);
		exit(EINVAL);
	}
	for (i = 0; i < t.refn; i++) {
		struct ref *f;
		f = &t.refs[i];
		f->data = db_read(f->hash);
		dir_writefile(str(f->name), f->data, outdir);
	}
	tree_destroy(&t);
	return ;
}

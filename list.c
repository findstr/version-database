#include <stdio.h>
#include "db.h"
#include "dr.h"
#include "object.h"
#include "list.h"

void
list(struct list_args *args)
{
	int i, n;
	struct release r;
	struct tree t;
	db_readrel(&r, args->hash);
	db_readtree(&t, r.tree);
	release_destroy(&r);
	n = t.refn;
	for (i = 0; i < n; i++) {
		printf("%s ==> %s\n",
			t.refs[i].name->buf,
			t.refs[i].hash->buf);
	}
	printf("total:%d\n", n);
	tree_destroy(&t);
	return ;
}


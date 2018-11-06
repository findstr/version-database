#include <stdio.h>
#include <time.h>
#include "db.h"
#include "object.h"
#include "history.h"

void
history()
{
	dr_t h;
	struct release rel;
	h = db_readhead(&rel, NULL);
	if (h->size == 0) {
		printf("Empty db\n");
	} else {
		for (;;) {
			struct tm *t;
			char datetime[200];
			t = localtime(&rel.time);
			strftime(datetime, sizeof(datetime),
				"%a %b %d %H:%M:%S %Z", t);
			printf("Version:%s\n", rel.ver->buf);
			printf("release:%s\n", h->buf);
			printf("Date:%s\n", datetime);
			printf("\n\t%s\n\n", rel.note->buf);
			if (rel.prev->size <= 0)
				break;
			dr_unref(h);
			h = dr_ref(rel.prev);
			release_destroy(&rel);
			memset(&rel, 0, sizeof(rel));
			db_readrel(&rel, h);
		}
	}
	dr_unref(h);
	release_destroy(&rel);
	return ;
}


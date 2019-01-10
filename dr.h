#ifndef _DR_H
#define _DR_H
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define str(dr)	((char *)(dr)->buf)

typedef struct dr_{	//data reference
	int ref;
	int size;
	uint8_t buf[1];
} *dr_t;

typedef struct drvec_ {
	int cap;
	int size;
	dr_t *list;
} *drvec_t;


static inline int
dr_cmp(dr_t a, dr_t b)
{
	if (a->size == b->size && (memcmp(a->buf, b->buf, a->size) == 0))
		return 0;
	return -1;
}

static inline dr_t
dr_ref(dr_t d)
{
	if (d != NULL)
		++d->ref;
	return d;
}

static inline void
dr_unref(dr_t d)
{
	if (d == NULL)
		return ;
	if (--d->ref <= 0) {
		assert(d->ref == 0);
		free(d);
		//printf("dr_unref:%p\n", d);
	}
}
static inline dr_t
dr_new(int size, const uint8_t *data)
{
	dr_t d;
	d = (dr_t)malloc(size + sizeof(struct dr_));
	d->ref = 1;
	d->size = size;
	if (data != NULL)
		memcpy(d->buf, data, size);
	d->buf[size] = 0;
	//printf("dr_new:%p\n", d);
	return d;
}

static inline dr_t
dr_resize(dr_t d, int size)
{
	if (d->ref == 1) {
		d = realloc(d, size + sizeof(struct dr_));
		d->size = size;
	} else {
		dr_t n = dr_new(size, NULL);
		if (size > d->size)
			size = d->size;
		d->size = size;
		memcpy(n->buf, d->buf, size);
		dr_unref(d);
		d = n;
	}
	return d;
}

static inline dr_t
dr_newstr(const char *str)
{
	dr_t d;
	int sz = strlen(str);
	d = dr_new(sz, (const uint8_t *)str);
	//printf("dr_new:%p\n", d);
	return d;
}

typedef struct {
	int size;
	dr_t seq;
}drb_t;

static inline void
drb_init(drb_t *drb, int size)
{
	drb->size = 0;
	drb->seq = dr_new(size, NULL);
}

static inline int
drb_size(drb_t *drb)
{
	return drb->size;
}

static inline uint8_t *
drb_check(drb_t *drb, int size)
{
	int seqsize = drb->seq->size;
	int needsize = drb->size + size;
	while (seqsize < needsize) {
		seqsize *= 2;
		drb->seq = dr_resize(drb->seq, seqsize);
	}
	return &drb->seq->buf[drb->size];
}

static inline void
drb_end(drb_t *drb, const uint8_t *p)
{
	drb->size = p - drb->seq->buf;
}

static inline dr_t
drb_dump(drb_t *drb)
{
	dr_t d;
	d = dr_resize(drb->seq, drb->size);
	drb->seq = NULL;
	return d;
}

#endif


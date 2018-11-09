#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "object.h"

#define MALLOC	malloc
#define FREE	free

static void
ref_destroy(struct ref *r)
{
	dr_unref(r->hash);
	dr_unref(r->name);
	dr_unref(r->data);
};

int
ref_hashcmp(const void *aa, const void *bb)
{
	struct ref *a = (struct ref *)aa;
	struct ref *b = (struct ref *)bb;
	return strcmp((char *)a->hash->buf, (char *)b->hash->buf);
}

int
ref_namecmp(const void *aa, const void *bb)
{
	struct ref *a = (struct ref *)aa;
	struct ref *b = (struct ref *)bb;
	return strcmp((char *)a->name->buf, (char *)b->name->buf);
}

dr_t
blob_marshal(struct blob *b)
{
	return dr_ref(b->data);
}

void
blob_unmarshal(struct blob *b, dr_t data)
{
	b->data = dr_ref(data);
	return ;
}

void
blob_destroy(struct blob *b)
{

}

dr_t
tree_marshal(struct tree *t)
{
	dr_t d;
	int i, sz;
	char *ptr;
	struct ref *refs = t->refs;
	sz =	4 +				//type
		4;				//refn
	for (i = 0; i < t->refn; i++) {
		struct ref *r = &refs[i];
		sz += 4 + r->hash->size + 4 + r->name->size;
	}
	d = dr_new(sz, NULL);
	ptr = (char*)d->buf;
	*(uint32_t *)ptr = TREE;			ptr += 4;
	*(uint32_t *)ptr = t->refn;			ptr += 4;
	for (i = 0; i < t->refn; i++) {
		struct ref *r = &refs[i];
		*(uint32_t *)ptr = r->hash->size;	ptr += 4;
		memcpy(ptr, r->hash->buf, r->hash->size);ptr += r->hash->size;
		*(uint32_t *)ptr = r->name->size;	ptr += 4;
		memcpy(ptr, r->name->buf, r->name->size);ptr += r->name->size;
	}
	return d;
}

void
tree_unmarshal(struct tree *t, dr_t d)
{
	int type, i, size;
	uint8_t *ptr = d->buf;
	type = *(uint32_t *)ptr;			ptr += 4;
	assert(type == TREE);
	t->refn = *(uint32_t *)ptr;			ptr += 4;
	size = t->refn * sizeof(struct ref);
	t->refs = MALLOC(size);
	memset(t->refs, 0, size);
	for (i = 0; i < t->refn; i++) {
		struct ref *r = &t->refs[i];
		size = *(uint32_t *)ptr;		ptr += 4;
		r->hash = dr_new(size, ptr);		ptr += size;
		size = *(uint32_t *)ptr;		ptr += 4;
		r->name = dr_new(size, ptr);		ptr += size;
	}
	return ;
}

void
tree_destroy(struct tree *t)
{
	int i;
	for (i = 0; i < t->refn; i++)
		ref_destroy(&t->refs[i]);
	FREE(t->refs);
	return ;
}

void
tree_sort(struct tree *t, compar_t cb)
{
	qsort(t->refs, t->refn, sizeof(struct ref), cb);
}

struct ref *
tree_search(struct tree *t, struct ref *k, compar_t cb)
{
	return bsearch(k, t->refs, t->refn, sizeof(*k), cb);
}


dr_t
release_marshal(struct release *r)
{
	dr_t d;
	int sz;
	uint8_t *ptr;
	sz =	4 +				//type
		sizeof(uint64_t) +		//time
		4 + r->prev->size +		//prev
		4 + r->tree->size +		//tree
		4 + r->ver->size +		//ver
		4 + r->note->size +		//note
		4 + r->finger->size;		//finger
	d = dr_new(sz, NULL);
	ptr = d->buf;
	*(uint32_t *)ptr = RELEASE;			ptr += 4;
	*(uint64_t *)ptr = r->time;			ptr += sizeof(uint64_t);
	*(uint32_t *)ptr = r->prev->size;		ptr += 4;
	memcpy(ptr, r->prev->buf, r->prev->size);	ptr += r->prev->size;
	*(uint32_t *)ptr = r->tree->size;		ptr += 4;
	memcpy(ptr, r->tree->buf, r->tree->size);	ptr += r->tree->size;
	*(uint32_t *)ptr = r->ver->size;		ptr += 4;
	memcpy(ptr, r->ver->buf, r->ver->size);		ptr += r->ver->size;
	*(uint32_t *)ptr = r->note->size;		ptr += 4;
	memcpy(ptr, r->note->buf, r->note->size);	ptr += r->note->size;
	*(uint32_t *)ptr = r->finger->size;		ptr += 4;
	memcpy(ptr, r->finger->buf, r->finger->size);
	return d;
}

void
release_unmarshal(struct release *r, dr_t d)
{
	int type, size;
	uint8_t *ptr = d->buf;
	type = *(uint32_t *)ptr;			ptr += 4;
	assert(type == RELEASE);
	r->time = *(uint64_t *)ptr;			ptr += sizeof(uint64_t);
	size = *(uint32_t *)ptr;			ptr += 4;
	r->prev = dr_new(size, ptr);			ptr += size;
	size = *(uint32_t *)ptr;			ptr += 4;
	r->tree = dr_new(size, ptr);			ptr += size;
	size = *(uint32_t *)ptr;			ptr += 4;
	r->ver = dr_new(size, ptr);			ptr += size;
	size = *(uint32_t *)ptr;			ptr += 4;
	r->note = dr_new(size, ptr);			ptr += size;
	size = *(uint32_t *)ptr;			ptr += 4;
	r->finger = dr_new(size, ptr);
	return ;
}

void
release_destroy(struct release *r)
{
	dr_unref(r->prev);
	dr_unref(r->tree);
	dr_unref(r->ver);
	dr_unref(r->note);
	return ;
}


#ifndef	_OBJECT_H
#define _OBJECT_H

#include "dr.h"

#define MC(a,b,c,d)	((d) << 24 | (c) << 16 | (b) << 8 | (a))
#define BLOB	MC('B', 'L', 'O', 'B')
#define TREE	MC('T', 'R', 'E', 'E')
#define RELEASE	MC('R', 'E', 'L', ' ')
#define FINGER	"_fingerprint"

struct blob {
	dr_t data;
};

struct ref {
	dr_t hash;
	dr_t name;
	//cache
	dr_t data;
};

struct tree {
	int refn;
	struct ref *refs;
};

struct release {
	time_t time;
	dr_t prev;
	dr_t tree;
	dr_t ver;
	dr_t note;
	dr_t finger;
};

typedef int (compar_t)(const void *a, const void *b);

int ref_hashcmp(const void *a, const void *b);
int ref_namecmp(const void *a, const void *b);

dr_t blob_marshal(struct blob *b);
void blob_unmarshal(struct blob *b, dr_t data);
void blob_destroy(struct blob *b);

dr_t tree_marshal(struct tree *t);
void tree_unmarshal(struct tree *t, dr_t data);
void tree_destroy(struct tree *t);
void tree_sort(struct tree *t, compar_t cb);
struct ref *tree_search(struct tree *t, struct ref *k, compar_t cb);

dr_t release_marshal(struct release *r);
void release_unmarshal(struct release *r, dr_t data);
void release_destroy(struct release *r);

#endif


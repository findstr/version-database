#ifndef _RELEASE_H
#define _RELEASE_H
#include "dr.h"

struct release_args {
	dr_t version;
	dr_t describe;
	dr_t fromdir;
};

void release(struct release_args *args);

#endif


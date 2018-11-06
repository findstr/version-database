#ifndef _PATCH_H
#define _PATCH_H
#include "dr.h"

struct patch_args {
	dr_t hash;
	dr_t patch;
	dr_t output;
};

void patch(struct patch_args *args);


#endif

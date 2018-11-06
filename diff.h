#ifndef _DIFF_H
#define _DIFF_H
#include "ctrl.h"
struct diff_args {
	dr_t a;
	dr_t b;
	dr_t p;
};

void diff(struct diff_args *args);

#endif


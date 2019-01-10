#ifndef _DIFF_H
#define _DIFF_H
struct diff_args {
	int r;
	dr_t a;
	dr_t b;
	dr_t o;
};

void diff(struct diff_args *args);

#endif


#ifndef _DIR_H
#define _DIR_H
#include "dr.h"

void dir_ensure(const char *path);
int dir_scan(const char *path, dr_t **res, int skip);
dr_t dir_readfile(const char *path, dr_t root);
void dir_writefile(const char *path, dr_t d, dr_t root);
void dir_rename(const char *from, const char *to, dr_t root);
void dir_remove(const char *path, dr_t root);
void dir_movdir(dr_t from, dr_t to);

#endif


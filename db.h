#ifndef _DB_H
#define _DB_H
#include "dr.h"
#include "object.h"

void db_init();

dr_t db_hash(dr_t d);
dr_t db_read(dr_t name);
dr_t db_tryread(dr_t name);
dr_t db_write(dr_t name, dr_t d);
void db_readrel(struct release *rel, dr_t hash);
void db_readtree(struct tree *tree, dr_t hash);
dr_t db_readhead(struct release *rel, struct tree *tree);
dr_t db_writehead(struct release *rel);
dr_t db_aliashash(dr_t a);

void db_init();

#endif


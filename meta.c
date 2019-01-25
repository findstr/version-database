#include <stdio.h>
#include "dr.h"
#include "meta.h"

#define HASH_LEN		uint8
#define NAME_LEN		uint16
#define HASH_LEN_SIZE		(sizeof(uint8_t))
#define NAME_LEN_SIZE		(sizeof(uint16_t))

static void
writeact(drb_t *drb, int act)
{
	uint8_t *p = drb_check(drb, 1);
	*p = (uint8_t)act; p += 1;
	drb_end(drb, p);
}

static dr_t
readname(const uint8_t **pp)
{
	int size;
	dr_t name;
	const uint8_t *p = *pp;
	size = NAME_LEN(p);	p += NAME_LEN_SIZE;
	name = dr_new(size, p); p += size;
	*pp = p;
	return name;
}

static void
writename(drb_t *drb, dr_t name)
{
	uint8_t *p;
	int size;
	p = drb_check(drb, NAME_LEN_SIZE + name->size);
	NAME_LEN(p) = size = name->size;	p += NAME_LEN_SIZE;
	memcpy(p, name->buf, size);		p += size;
	drb_end(drb, p);
}

static dr_t
readdata(const uint8_t **pp)
{
	int size;
	dr_t data;
	const uint8_t *p = *pp;
	size = uint32(p);	p += 4;
	data = dr_new(size, p);	p += size;
	*pp = p;
	return data;
}

static void
writedata(drb_t *drb, dr_t data)
{
	int size;
	uint8_t *p;
	p = drb_check(drb, data->size);
	uint32(p) = size = data->size;		p += 4;
	memcpy(p, data->buf, size);		p += size;
	drb_end(drb, p);
	return ;
}

void
ctrl_new(drb_t *drb, struct NEW *c)
{
	writeact(drb, CTRL_NEW);
	writename(drb, c->name);
	writedata(drb, c->data);
	printf(".NEW %s size:%.3f KiB\n",
		c->name->buf, c->data->size/1024.f);
	return ;
}

static const uint8_t *
ctrl_new_read(const uint8_t *p, struct NEW *c)
{
	c->name = readname(&p);
	c->data = readdata(&p);
	printf(".NEW %s size:%.3f KiB\n",
		c->name->buf, c->data->size/1024.f);
	return p;
}

static void
ctrl_new_free(struct NEW *n)
{
	dr_unref(n->name);
	dr_unref(n->data);
}


void
ctrl_dff(drb_t *drb, struct DFF *c)
{
	writeact(drb, CTRL_DFF);
	writename(drb, c->name);
	writedata(drb, c->patch);
	printf(".DFF %s patch:%d Byte\n",
		c->name->buf, c->patch->size);
	return ;
}

static const uint8_t *
ctrl_dff_read(const uint8_t *p, struct DFF *c)
{
	c->name = readname(&p);
	c->patch = readdata(&p);
	printf(".DFF %s patch:%d Byte\n",
		c->name->buf, c->patch->size);
	return p;
}

static void
ctrl_dff_free(struct DFF *n)
{
	dr_unref(n->name);
	dr_unref(n->patch);
}

void
ctrl_dfx(drb_t *drb, struct DFX *c)
{
	writeact(drb, CTRL_DFX);
	writename(drb, c->name);
	writename(drb, c->namea);
	writedata(drb, c->patch);
	printf(".DFX %s -> %s patch:%d Byte\n",
		c->namea->buf, c->name->buf, c->patch->size);
	return ;
}

static const uint8_t *
ctrl_dfx_read(const uint8_t *p, struct DFX *c)
{
	c->name = readname(&p);
	c->namea = readname(&p);
	c->patch = readdata(&p);
	printf(".DFX %s -> %s patch:%d Byte\n",
		c->namea->buf, c->name->buf, c->patch->size);
	return p;
}

static void
ctrl_dfx_free(struct DFX *n)
{
	dr_unref(n->name);
	dr_unref(n->namea);
	dr_unref(n->patch);
}


void
ctrl_del(drb_t *drb, struct DEL *c)
{
	writeact(drb, CTRL_DEL);
	writename(drb, c->name);
	printf(".DEL %s\n", c->name->buf);
}

static const uint8_t *
ctrl_del_read(const uint8_t *p, struct DEL *n)
{
	n->name = readname(&p);
	printf(".DEL %s\n", n->name->buf);
	return p;
}

static void
ctrl_del_free(struct DEL *n)
{
	dr_unref(n->name);
}

void
ctrl_cpy(drb_t *drb, struct CPY *c)
{
	writeact(drb, CTRL_CPY);
	writename(drb, c->name);
	writename(drb, c->namea);
	printf(".CPY %s => %s\n", c->namea->buf, c->name->buf);
}

static const uint8_t *
ctrl_cpy_read(const uint8_t *p, struct CPY *n)
{
	n->name = readname(&p);
	n->namea = readname(&p);
	return p;
}

static void
ctrl_cpy_free(struct CPY *n)
{
	dr_unref(n->name);
	dr_unref(n->namea);
}


const uint8_t *
ctrl_read(const uint8_t *p, struct CTRL *ctrl)
{
	ctrl->act = uint8(p);	p += 1;
	switch (ctrl->act) {
	case CTRL_NEW:
		return ctrl_new_read(p, &ctrl->u.new);
	case CTRL_DFF:
		return ctrl_dff_read(p, &ctrl->u.dff);
	case CTRL_DFX:
		return ctrl_dfx_read(p, &ctrl->u.dfx);
	case CTRL_CPY:
		return ctrl_cpy_read(p, &ctrl->u.cpy);
	case CTRL_DEL:
		return ctrl_del_read(p, &ctrl->u.del);
	default:
		assert(0);
	}
	return NULL;
}

void
ctrl_destroy(struct CTRL *ctrl)
{
	switch (ctrl->act) {
	case CTRL_NEW:
		ctrl_new_free(&ctrl->u.new);
		break;
	case CTRL_DFF:
		ctrl_dff_free(&ctrl->u.dff);
		break;
	case CTRL_DFX:
		ctrl_dfx_free(&ctrl->u.dfx);
		break;
	case CTRL_CPY:
		ctrl_cpy_free(&ctrl->u.cpy);
		break;
	case CTRL_DEL:
		ctrl_del_free(&ctrl->u.del);
		break;
	default:
		assert(0);
	}
	return ;
}

void
patch_total(drb_t *drb, int size)
{
	uint8_t *p = drb_check(drb, 4);
	uint32(p) = size;	p += 4;
	drb_end(drb, p);
}

const uint8_t *
patch_total_read(const uint8_t *p, uint32_t *size)
{
	*size = uint32(p); p += 4;
	return p;
}

void
patch_copy(drb_t *drb, struct COPY *copy)
{
	uint8_t *p = drb_check(drb, 1+4+4);
	uint8(p) = PATCH_COPY;	p += 1;
	uint32(p) = copy->pos;	p += 4;
	uint32(p) = copy->size;	p += 4;
	drb_end(drb, p);
	return ;
}

void
patch_insert(drb_t *drb, struct INSERT *insert)
{
	int size = insert->size;
	uint8_t *p = drb_check(drb, 1+4+size);
	uint8(p) = PATCH_INSERT;	p += 1;
	uint32(p) = size;		p += 4;
	memcpy(p, insert->p, size);	p += size;
	drb_end(drb, p);
}


static const uint8_t *
patch_copy_read(const uint8_t *p, struct COPY *copy)
{
	copy->pos = uint32(p);	p += 4;
	copy->size = uint32(p); p += 4;
	return p;
}

static const uint8_t *
patch_insert_read(const uint8_t *p, struct INSERT *insert)
{
	insert->size = uint32(p);	p += 4;
	insert->p = p;			p += insert->size;
	return p;
}

const uint8_t *
patch_read(const uint8_t *p, struct PATCH *patch)
{
	patch->act = uint8(p);		p += 1;
	if (patch->act == PATCH_COPY)
		return patch_copy_read(p, &patch->u.copy);
	else if (patch->act == PATCH_INSERT)
		return patch_insert_read(p, &patch->u.insert);
	else
		assert(0);
	return NULL;
}




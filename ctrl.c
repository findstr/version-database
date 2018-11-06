#include <stdio.h>
#include "dr.h"
#include "ctrl.h"

#define HASH_LEN		uint16
#define NAME_LEN		uint16
#define HASH_LEN_SIZE		(sizeof(uint16_t))
#define NAME_LEN_SIZE		(sizeof(uint16_t))

void
ctrl_new(drb_t *drb, struct NEW *c)
{
	int size;
	uint8_t *p;
	p = drb_check(drb,1 +
		HASH_LEN_SIZE + c->hash->size +
		NAME_LEN_SIZE + c->name->size +
		4 + c->data->size);
	uint8(p) = CTRL_NEW;			p += 1;
	HASH_LEN(p) = size = c->hash->size;	p += HASH_LEN_SIZE;
	memcpy(p, c->hash->buf, size);		p += size;
	NAME_LEN(p) = size = c->name->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->name->buf, size);		p += size;
	uint32(p) = size = c->data->size;	p += 4;
	memcpy(p, c->data->buf, size);		p += size;
	drb_end(drb, p);
	printf(".NEW %s hash:%s size:%.3f KiB\n", c->name->buf,
		c->hash->buf, c->data->size/1024.f);
	return ;
}

static const uint8_t *
ctrl_new_read(const uint8_t *p, struct NEW *c)
{
	int size;
	size = HASH_LEN(p);		p += HASH_LEN_SIZE;
	c->hash = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	c->name = dr_new(size, p);	p += size;
	size = uint32(p);		p += 4;
	c->data = dr_new(size, p);	p += size;
	printf(".NEW %s hash:%s size:%.3f KiB\n", c->name->buf,
		c->hash->buf, c->data->size/1024.f);
	return p;
}

static void
ctrl_new_free(struct NEW *n)
{
	dr_unref(n->hash);
	dr_unref(n->name);
	dr_unref(n->data);
}


void
ctrl_dff(drb_t *drb, struct DFF *c)
{
	int size;
	uint8_t *p;
	p = drb_check(drb,1 +
		HASH_LEN_SIZE + c->hash->size +
		NAME_LEN_SIZE + c->name->size +
		4 + c->patch->size);
	uint8(p) = CTRL_DFF;			p += 1;
	HASH_LEN(p) = size = c->hash->size;	p += HASH_LEN_SIZE;
	memcpy(p, c->hash->buf, size);		p += size;
	NAME_LEN(p) = size = c->name->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->name->buf, size);		p += size;
	uint32(p) = size = c->patch->size;	p += 4;
	memcpy(p, c->patch->buf, size);		p += size;
	drb_end(drb, p);
	printf(".DFF %s hash:%s patch:%d Byte\n", c->name->buf,
		c->hash->buf, c->patch->size);
	return ;
}

static const uint8_t *
ctrl_dff_read(const uint8_t *p, struct DFF *c)
{
	int size;
	size = HASH_LEN(p);		p += HASH_LEN_SIZE;
	c->hash = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	c->name = dr_new(size, p);	p += size;
	size = uint32(p);		p += 4;
	c->patch = dr_new(size, p);	p += size;
	printf(".DFF %s hash:%s patch:%d Byte\n", c->name->buf,
		c->hash->buf, c->patch->size);
	return p;
}

static void
ctrl_dff_free(struct DFF *n)
{
	dr_unref(n->hash);
	dr_unref(n->name);
	dr_unref(n->patch);
}

void
ctrl_dfx(drb_t *drb, struct DFX *c)
{
	int size;
	uint8_t *p;
	p = drb_check(drb,1 +
		HASH_LEN_SIZE + c->hash->size +
		NAME_LEN_SIZE + c->name->size +
		NAME_LEN_SIZE + c->namea->size +
		4 + c->patch->size);
	uint8(p) = CTRL_DFX;			p += 1;
	HASH_LEN(p) = size = c->hash->size;	p += HASH_LEN_SIZE;
	memcpy(p, c->hash->buf, size);		p += size;
	NAME_LEN(p) = size = c->name->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->name->buf, size);		p += size;
	NAME_LEN(p) = size = c->namea->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->namea->buf, size);		p += size;
	uint32(p) = size = c->patch->size;	p += 4;
	memcpy(p, c->patch->buf, size);		p += size;
	drb_end(drb, p);
	printf(".DFX %s -> %s hash:%s patch:%d Byte\n", c->namea->buf,
		c->name->buf, c->hash->buf, c->patch->size);
	return ;
}

static const uint8_t *
ctrl_dfx_read(const uint8_t *p, struct DFX *c)
{
	int size;
	size = HASH_LEN(p);		p += HASH_LEN_SIZE;
	c->hash = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	c->name = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	c->namea = dr_new(size, p);	p += size;
	size = uint32(p);		p += 4;
	c->patch = dr_new(size, p);	p += size;
	printf(".DFX %s -> %s hash:%s patch:%d Byte\n", c->namea->buf,
		c->name->buf, c->hash->buf, c->patch->size);
	return p;
}

static void
ctrl_dfx_free(struct DFX *n)
{
	dr_unref(n->hash);
	dr_unref(n->name);
	dr_unref(n->namea);
	dr_unref(n->patch);
}


void
ctrl_del(drb_t *drb, struct DEL *c)
{
	int size;
	uint8_t *p;
	p = drb_check(drb,1 + NAME_LEN_SIZE + c->name->size);
	uint8(p) = CTRL_DEL;			p += 1;
	NAME_LEN(p) = size = c->name->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->name->buf, size);		p += size;
	drb_end(drb, p);
	printf(".DEL %s\n", c->name->buf);
}

static const uint8_t *
ctrl_del_read(const uint8_t *p, struct DEL *n)
{
	int size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	n->name = dr_new(size, p);	p += size;
	printf(".DEL %s\n", n->name->buf);
	return p;
}

static void
ctrl_del_free(struct DEL *n)
{
	dr_unref(n->name);
}

void
ctrl_mov(drb_t *drb, struct MOV *c)
{
	int size;
	uint8_t *p;
	p = drb_check(drb,1 +
		HASH_LEN_SIZE + c->hash->size +
		NAME_LEN_SIZE + c->name->size +
		NAME_LEN_SIZE + c->namea->size);
	uint8(p) = CTRL_MOV;			p += 1;
	HASH_LEN(p) = size = c->hash->size;	p += HASH_LEN_SIZE;
	memcpy(p, c->hash->buf, size);		p += size;
	NAME_LEN(p) = size = c->name->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->name->buf, size);		p += size;
	NAME_LEN(p) = size = c->namea->size;	p += NAME_LEN_SIZE;
	memcpy(p, c->namea->buf, size);		p += size;
	drb_end(drb, p);
	printf(".MOV %s => %s\n", c->namea->buf, c->name->buf);
}

static const uint8_t *
ctrl_mov_read(const uint8_t *p, struct MOV *n)
{
	int size;
	size = HASH_LEN(p);		p += HASH_LEN_SIZE;
	n->hash = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	n->name = dr_new(size, p);	p += size;
	size = NAME_LEN(p);		p += NAME_LEN_SIZE;
	n->namea = dr_new(size, p);	p += size;
	return p;
}

static void
ctrl_mov_free(struct MOV *n)
{
	dr_unref(n->hash);
	dr_unref(n->name);
	dr_unref(n->namea);
}


const uint8_t *
ctrl_read(const uint8_t *p, struct CTRL *ctrl)
{
	ctrl->type = uint8(p);	p += 1;
	switch (ctrl->type) {
	case CTRL_NEW:
		return ctrl_new_read(p, &ctrl->u.new);
	case CTRL_DFF:
		return ctrl_dff_read(p, &ctrl->u.dff);
	case CTRL_DFX:
		return ctrl_dfx_read(p, &ctrl->u.dfx);
	case CTRL_MOV:
		return ctrl_mov_read(p, &ctrl->u.mov);
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
	switch (ctrl->type) {
	case CTRL_NEW:
		ctrl_new_free(&ctrl->u.new);
		break;
	case CTRL_DFF:
		ctrl_dff_free(&ctrl->u.dff);
		break;
	case CTRL_DFX:
		ctrl_dfx_free(&ctrl->u.dfx);
		break;
	case CTRL_MOV:
		ctrl_mov_free(&ctrl->u.mov);
		break;
	case CTRL_DEL:
		ctrl_del_free(&ctrl->u.del);
		break;
	default:
		assert(0);
	}
	return ;

}


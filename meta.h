#ifndef _META_H
#define _META_H
#include <stdint.h>

#define CTRL_NEW		(0x10)
#define	CTRL_DFF		(0x20)
#define CTRL_DFX		(0x30)
#define CTRL_MOV		(0x40)
#define CTRL_DEL		(0x50)


#define PATCH_COPY		(0x01)
#define PATCH_INSERT		(0x02)

#define uint8(p)	(*(uint8_t *)(p))
#define uint16(p)	(*(uint16_t *)(p))
#define uint32(p)	(*(uint32_t *)(p))


struct NEW {
	dr_t name;
	dr_t data;
};

struct DFF {
	dr_t name;
	dr_t patch;
};

struct DFX {
	dr_t name;
	dr_t namea;
	dr_t patch;
};

struct DEL {
	dr_t name;
};

struct MOV {
	dr_t name;
	dr_t namea;
};

struct CTRL {
	uint8_t act;
	union {
		struct NEW new;
		struct DFF dff;
		struct DFX dfx;
		struct DEL del;
		struct MOV mov;
	} u;
};

struct COPY {
	uint32_t pos;
	uint32_t size;
};

struct INSERT {
	const uint8_t *p;
	uint32_t size;
};

struct PATCH {
	uint8_t act;
	union {
		struct COPY copy;
		struct INSERT insert;
	} u;
};

void ctrl_new(drb_t *drb, struct NEW *c);
void ctrl_dff(drb_t *drb, struct DFF *c);
void ctrl_dfx(drb_t *drb, struct DFX *c);
void ctrl_del(drb_t *drb, struct DEL *c);
void ctrl_mov(drb_t *drb, struct MOV *c);
void ctrl_destroy(struct CTRL *ctrl);

void patch_total(drb_t *drb, int size);
void patch_copy(drb_t *drb, struct COPY *copy);
void patch_insert(drb_t *drb, struct INSERT *insert);

const uint8_t *ctrl_read(const uint8_t *p, struct CTRL *ctrl);
const uint8_t *patch_total_read(const uint8_t *p, uint32_t *size);
const uint8_t *patch_read(const uint8_t *p, struct PATCH *patch);

#endif


#include <stdio.h>
#include <errno.h>
#include "dr.h"
#include "object.h"
#include "meta.h"
#include "dir.h"
#include "db.h"
#include "checkout.h"
#include "patch.h"

#define TEMP	"_temp"

static dr_t
patch_content(dr_t old, dr_t patch)
{
	dr_t new;
	uint8_t *w;
	uint32_t size;
	const uint8_t *p = patch->buf;
	const uint8_t *e = p + patch->size;
	p = patch_total_read(p, &size);
	new = dr_new(size, NULL);
	w = new->buf;
	printf("patch size:%d total:%d\n", patch->size, size);
	while (p < e) {
		struct PATCH patch;
		p = patch_read(p, &patch);
		if (patch.act == PATCH_COPY) {
			uint32_t pos, size;
			pos = patch.u.copy.pos;
			size = patch.u.copy.size;
			printf("COPY FROM %d TO %ld SIZE %d\n",
				pos, w - new->buf, size);
			assert(new->size - (w - new->buf) >= size);
			memcpy(w, &old->buf[pos], size);
			w += size;
		} else if (patch.act == PATCH_INSERT) {
			int sz = patch.u.insert.size;
			assert(new->size - (w - new->buf) >= sz);
			memcpy(w, patch.u.insert.p, sz);
			w += sz;
		} else {
			assert(0);
		}
	}
	return new;
}

void
patch(struct patch_args *args)
{
	dr_t patch;
	dr_t outdir, outtemp = NULL;
	int size, dfn = 0;
	const uint8_t *p, *e;
	outdir = args->output;
	checkout(args->hash, outdir);
	patch = dir_readfile(str(args->patch), NULL);
	if (patch == NULL) {
		fprintf(stderr, "patch file '%s' %s\n",
			args->patch->buf, strerror(errno));
		exit(EINVAL);
	}
	//prepare temp file
	printf("DFF/DFX\n");
	size = outdir->size;
	outtemp = dr_new(size + 1 + sizeof(TEMP) - 1, NULL);
	memcpy(outtemp->buf, outdir->buf, size);
	outtemp->buf[size] = '/';
	memcpy(&outtemp->buf[size+1], TEMP, sizeof(TEMP) - 1);
	p = patch->buf;
	e = p + patch->size;
	while (p < e) {
		struct CTRL ctrl;
		dr_t namea, patchd, oldfile = NULL;
		dr_t hash = NULL, newfile = NULL;
		printf("offset:%ld\n", p - patch->buf);
		p = ctrl_read(p, &ctrl);
		switch (ctrl.act) {
		case CTRL_DFF:
			namea = ctrl.u.dff.name;
			patchd = ctrl.u.dff.patch;
			goto diff;
		case CTRL_DFX:
			namea = ctrl.u.dfx.namea;
			patchd = ctrl.u.dfx.patch;
			diff:
			++dfn;
			oldfile = dir_readfile(str(namea), outdir);
			newfile = patch_content(oldfile, patchd);
			printf("ref:%d\n", newfile->ref);
			hash = db_hash(newfile);
			assert(dr_cmp(hash, ctrl.u.dff.hash) == 0);
			dir_writefile(str(ctrl.u.dff.name), newfile, outtemp);
			break;
		case CTRL_MOV:
		case CTRL_NEW:
		case CTRL_DEL:
			break;
		}
		dr_unref(hash);
		dr_unref(oldfile);
		dr_unref(newfile);
		ctrl_destroy(&ctrl);
	}
	printf("NEW/MOV/DEL\n");
	p = patch->buf;
	e = p + patch->size;
	while (p < e) {
		struct CTRL ctrl;
		dr_t namea = NULL, oldfile = NULL;
		dr_t hash = NULL, newfile = NULL;
		p = ctrl_read(p, &ctrl);
		switch (ctrl.act) {
		case CTRL_NEW:
			hash = db_hash(ctrl.u.new.data);
			assert(dr_cmp(hash, ctrl.u.new.hash) == 0);
			dir_writefile(str(ctrl.u.new.name),
				ctrl.u.new.data, outdir);
			break;
		case CTRL_MOV:
			oldfile = dir_readfile(str(namea), outdir);
			hash = db_hash(oldfile);
			assert(dr_cmp(hash, ctrl.u.mov.hash) == 0);
			dir_rename(str(namea), str(ctrl.u.mov.name), outdir);
			break;
		case CTRL_DEL:
			dir_remove(str(ctrl.u.del.name), outdir);
			break;
		}
		dr_unref(hash);
		dr_unref(oldfile);
		dr_unref(newfile);
		ctrl_destroy(&ctrl);
	}
	printf("apply DFF/DFX result\n");
	if (dfn > 0)
		dir_movdir(outtemp, outdir);
	dr_unref(patch);
	dr_unref(outtemp);
	return ;
}

#ifdef MAIN

int main(int argc, const char *argv[])
{
	dr_t old, patch, new;
	if (argc < 3) {
		fprintf(stderr, "USAGE: %s oldfile patch\n", argv[0]);
		exit(0);
	}
	old = dir_readfile(argv[1], NULL);
	if (old == NULL) {
		fprintf(stderr, "ERROR: file %s nonexist\n", argv[1]);
		exit(errno);
	}
	patch = dir_readfile(argv[2], NULL);
	if (patch == NULL) {
		fprintf(stderr, "ERROR: file %s nonexist\n", argv[2]);
		exit(errno);
	}
	new = patch_content(old, patch);
	dir_writefile("a.out", new, NULL);
	dr_unref(old);
	dr_unref(patch);
	dr_unref(new);
	return 0;
}

#endif

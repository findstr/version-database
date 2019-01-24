#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "dr.h"
#include "db.h"
#include "release.h"
#include "history.h"
#include "list.h"
#include "checkout.h"
#include "diff.h"
#include "patch.h"

#define ARRAYSIZE(arr)	(sizeof(arr)/sizeof(arr[0]))

typedef void (cmd_t)(int argc, char *argv[]);
static char *EXEC = NULL;
struct subcmd {
	const char *cmd;
	cmd_t *handler;
};

static void help(int argc, char *argv[]);

static void
init_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	db_init();
}

static void
history_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	history();
}

static void
list_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	struct list_args args;
	if (argc < 2) {
		fprintf(stderr, "usage: %s list hash\n", EXEC);
		exit(EINVAL);
	}
	args.hash = dr_newstr(argv[1]);
	list(&args);
	dr_unref(args.hash);
	return ;
}

static void
release_h(int argc, char *argv[])
{
	int c;
	struct release_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "c:v:m:d:?")) != -1) {
		switch (c) {
		case 'c':
			args.checkout = dr_newstr(optarg);
			break;
		case 'v':
			args.version = dr_newstr(optarg);
			break;
		case 'm':
			args.describe = dr_newstr(optarg);
			break;
		case 'd':
			args.fromdir = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (args.version == NULL || args.describe == NULL) {
		fprintf(stderr, "usage: %s release -v version -m releasenote -d dir\n",
			EXEC);
		exit(EINVAL);
	}
	if (args.fromdir == NULL)
		args.fromdir = dr_newstr(".");
	release(&args);
	dr_unref(args.version);
	dr_unref(args.describe);
	dr_unref(args.fromdir);
	dr_unref(args.checkout);
}

static void
checkout_h(int argc, char *argv[])
{
	dr_t hash, outdir;
	if (argc < 3) {
		fprintf(stderr, "usage: %s checkout hash outdir\n", EXEC);
		exit(EINVAL);
	}
	hash = dr_newstr(argv[1]);
	outdir = dr_newstr(argv[2]);
	checkout(hash, outdir);
	dr_unref(hash);
	dr_unref(outdir);
	return ;
}

static void
diff_h(int argc, char *argv[])
{
	int c;
	struct diff_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "ra:b:o:?")) != -1){
		switch (c) {
		case 'r':
			args.r = 1;
			break;
		case 'a':
			args.a = dr_newstr(optarg);
			break;
		case 'b':
			args.b = dr_newstr(optarg);
			break;
		case 'o':
			args.o = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (!(args.o && ((args.a && args.b) || args.r == 1))) {
		fprintf(stderr, "usage: \n"
			"\t%s diff -a from -b to -o dir\n"
			"\t%s diff -r -o dir\n",
			EXEC, EXEC);
		exit(EINVAL);
	}
	diff(&args);
	dr_unref(args.a);
	dr_unref(args.b);
	dr_unref(args.o);
	return ;
}

static void
patch_h(int argc, char *argv[])
{
	int c;
	struct patch_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "a:p:o:?")) != -1) {
		switch (c) {
		case 'a':
			args.hash = dr_newstr(optarg);
			break;
		case 'p':
			args.patch = dr_newstr(optarg);
			break;
		case 'o':
			args.output = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (!(args.hash && args.patch && args.output)) {
		fprintf(stderr, "usage: %s patch -a hash -p patch -o output\n",
			argv[0]);
		exit(EINVAL);
	}
	patch(&args);
	dr_unref(args.hash);
	dr_unref(args.patch);
	dr_unref(args.output);
	return ;
}

static struct subcmd cmds[] = {
	{"help", help},
	{"init", init_h},
	{"history", history_h},
	{"list", list_h},
	{"release", release_h},
	{"checkout", checkout_h},
	{"diff", diff_h},
	{"patch", patch_h},
};

static void
help(int argc, char *argv[])
{
	int i;
	(void)argc;
	(void)argv;
	fprintf(stdout, "sub command list:\n");
	for (i = 0; i < ARRAYSIZE(cmds); i++) {
		fprintf(stdout, "%s\n", cmds[i].cmd);
	}
}

int main(int argc, char *argv[])
{
	int i;
	if (argc < 2)
		goto over;
	EXEC = argv[0];
	for (i = 0; i < ARRAYSIZE(cmds); i++) {
		if (strcmp(cmds[i].cmd, argv[1]) == 0) {
			cmds[i].handler(argc-1, &argv[1]);
			break;
		}
	}
	if (i >= ARRAYSIZE(cmds)) {
		over:
		fprintf(stderr, "usage: %s <command> [<args>]\n", argv[0]);
		exit(EINVAL);
	}
	return 0;
}


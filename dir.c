#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "conf.h"
#include "dr.h"
#include "dir.h"

struct args {
	int num;
	int cap;
	int prefix;
	dr_t *list;
};

static DIR *
scanbegin(const char *path)
{
	DIR *dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "FATAL: dir scan %s error:%s\n",
			path, strerror(errno));
		exit(errno);
	}
	return dir;
}

static void
scanend(DIR *dir)
{
	closedir(dir);
}

static void
scan_r(const char *prefix, struct args *args)
{
	DIR *dir;
	struct stat st;
	struct dirent *ent;
	dir = scanbegin(prefix);
	while ((ent = readdir(dir))) {
		char buf[PATH_MAX];
		if (ent->d_name[0] == '.')
			continue;
		snprintf(buf, PATH_MAX, "%s/%s", prefix, ent->d_name);
		stat(buf, &st);
		if (S_ISDIR(st.st_mode)) {
			scan_r(buf, args);
		} else if (S_ISREG(st.st_mode)) {
			args->list[args->num++] = dr_newstr(buf + args->prefix);
			if (args->num >= args->cap) {
				int size;
				args->cap *= 2;
				size = args->cap * sizeof(dr_t);
				args->list = realloc(args->list, size);
			}
		}
	}
	scanend(dir);
	return ;
}

int
dir_scan(const char *path, dr_t **res, int skip)
{
	struct args args;
	args.num = 0;
	args.cap = 128;
	if (skip)
		args.prefix = strlen(path) + 1;
	else
		args.prefix = 0;
	args.list = malloc(128 * sizeof(dr_t));
	scan_r(path, &args);
	if (res == NULL) {
		int i;
		for (i = 0; i < args.num; i++)
			dr_unref(args.list[i]);
		free(args.list);
	} else {
		*res = args.list;
	}
	return args.num;
}

static const char *
changedir(char buf[PATH_MAX], const char *path, dr_t root)
{
	if (root != NULL) {
		int n = strlen(path);
		int sz = root->size;
		memcpy(buf, root->buf, sz);
		if (buf[sz-1] != '/') {
			buf[sz] = '/';
			sz += 1;
		}
		memcpy(&buf[sz], path, n);
		buf[sz + n] = 0;
		path = buf;
	}
	return path;
}

void
dir_ensure(const char *path)
{
	char buf[PATH_MAX];
	char *p, *s, *e;
	strcpy(buf, path);
	path = buf;
	p = s = (char *)path;
	errno = 0;
	while ((e = strchr(s, '/'))) {
		int ret;
		*e = 0;
		errno = 0;
		ret = MKDIR(p);
		if (ret == -1 && errno != EEXIST) {
			fprintf(stderr, "mkdir '%s' %s\n", p, strerror(errno));
			exit(errno);
		}
		*e = '/';
		s = e + 1;
	}
}

dr_t
dir_readfile(const char *path, dr_t root)
{
	int fd;
	dr_t content;
	char buf[PATH_MAX];
	struct stat stat;
	path = changedir(buf, path, root);
	fd = open(path, O_RDONLY | O_BINARY);
	if (fd < 0)
		return NULL;
	fstat(fd, &stat);
	content = dr_new(stat.st_size, NULL);
	read(fd, content->buf, stat.st_size);
	close(fd);
	return content;
}

void
dir_writefile(const char *path, dr_t d, dr_t root)
{
	int fd, ret;
	char buf[PATH_MAX];
	path = changedir(buf, path, root);
	dir_ensure(path);
	fd = open(path, O_CREAT | O_RDWR | O_TRUNC | O_BINARY, 0644);
	if (fd < 0) {
		fprintf(stderr, "FATAL: write %s errno:%s\n",
			path, strerror(errno));
		exit(errno);
	}
	ret = write(fd, d->buf, d->size);
	assert(ret == d->size);
	close(fd);
	return ;
}

void
dir_rename(const char *from, const char *to, dr_t root)
{
	int ret;
	char fbuf[PATH_MAX], tbuf[PATH_MAX];
	from = changedir(fbuf, from, root);
	to = changedir(tbuf, to, root);
	dir_ensure(to);
	ret = rename(from, to);
	assert(ret == 0);
	return ;
}

void
dir_remove(const char *path, dr_t root)
{
	char buf[PATH_MAX];
	path = changedir(buf, path, root);
	remove(path);
	return ;
}

void
dir_movdir(dr_t from, dr_t to)
{
	int i, n, ret;
	int flen, tlen;
	dr_t *list;
	char buf[PATH_MAX];
	n = dir_scan(str(from), &list, 0);
	flen = from->size;
	tlen = to->size;
	memcpy(buf, to->buf, tlen);
	for (i = 0; i < n; i++) {
		dr_t d = list[i];
		memcpy(&buf[tlen], &d->buf[flen], d->size - flen);
		buf[d->size - flen + tlen] = 0;
		dir_ensure(buf);
		ret = remove(buf);
		assert(ret == 0 || (ret == -1 && errno == ENOENT));
		ret = rename((char *)d->buf, buf);
		assert(ret == 0);
		dr_unref(d);
	}
	free(list);
	return ;
}


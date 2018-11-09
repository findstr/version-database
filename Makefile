.PHONY:all

SRC:=main.c release.c history.c diff.c patch.c list.c checkout.c\
		dir.c db.c object.c meta.c\
		dc3.c sha256.c

all:vdb

vdb:$(SRC)
	gcc -g3 -Wall -Werror -o $@ $^


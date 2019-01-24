.PHONY:all

SRC:=main.c release.c history.c diff.c patch.c list.c checkout.c\
		dir.c db.c object.c meta.c\
		dc3.c sha1.c

all:vdb

win: LIB := -lws2_32 -lwsock32

win:all


vdb:$(SRC)
	gcc -g3 -fsigned-char -Wall -Werror -o $@ $^ $(LIB)


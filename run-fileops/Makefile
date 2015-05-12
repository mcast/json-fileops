
CFLAGS=-DJSMN_PARENT_LINKS

all: fileops-run-jsonl

fileops-run-jsonl: fileops-run-jsonl.o jsmn/jsmn.o jsmn/jsmn_util.o

jsmn/jsmn.o: jsmn/jsmn.c jsmn/jsmn.h
jsmn/jsmn_util.o: jsmn/jsmn_util.c jsmn/jsmn_util.h jsmn/jsmn.h

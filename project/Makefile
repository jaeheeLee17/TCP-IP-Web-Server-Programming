CC=gcc
CFLAGS=-g -o
TARGETS=myserver
all: $(TARGETS)
.PHONY: all
%:
	$(CC) $(CFLAGS) $@ $@.c
clean:
	rm $(TARGETS)

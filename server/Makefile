# Authors:
#   Andre Luis Raposo Marinho 93687
#    Rodrigo Resendes Pedro 93753    
# SO-Project MakeFile


SOURCES = main.c fs.c lock.c
SOURCES+= lib/bst.c lib/hash.c lib/inodes.c
OBJS = $(SOURCES:%.c=%.o)
CC   = gcc
LD   = gcc
CFLAGS =-Wall -std=gnu99 -I../ -g
LDFLAGS=-lm -pthread
TARGETS = tecnicofs

.PHONY: all clean

all: $(TARGETS)

$(TARGETS):
	$(LD) $(CFLAGS) $^ -o $@ $(LDFLAGS)


lib/bst.o: lib/bst.c lib/bst.h
lib/hash.o: lib/hash.c lib/hash.h
fs.o: fs.c fs.h lib/bst.h lib/hash.h lock.h tecnicofs-api-constants.h lib/inodes.h
lock.o: lock.c lock.h

main.o: main.c fs.h lock.h lib/timer.h constants.h lib/inodes.h
tecnicofs: lib/bst.o lib/hash.o fs.o lock.o lib/inodes.o main.o
lib/inodes.o: lib/inodes.c lib/inodes.h tecnicofs-api-constants.h

%.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)
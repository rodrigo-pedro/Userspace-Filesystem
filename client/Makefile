# Authors:
#   Andre Luis Raposo Marinho 93687
#    Rodrigo Resendes Pedro 93753    
# SO-Project MakeFile


SOURCES = tecnicofs-client-api.c
OBJS = $(SOURCES:%.c=%.o)
CC   = gcc
LD   = gcc
CFLAGS =-Wall -std=gnu99 -I../ -g
TARGETS = client

.PHONY: all clean

all: $(TARGETS)

$(TARGETS):
	$(LD) $(CFLAGS) $^ -o $@


tecnicofs-client-api.o: tecnicofs-client-api.c tecnicofs-client-api.h tecnicofs-api-constants.h

client: tecnicofs-client-api.o

%.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)
TARGETS = $(patsubst %.c, %, $(wildcard *.c))

ROOT_SRC = ../src
ROOT_LIB = ../lib
OBJS = $(wildcard ../obj/*.o)
LIB_OBJS = $(filter-out ../obj/main.o, $(OBJS))

CC = gcc
CFLAGS = -g -Wall

INCLUDES = \
	-I$(ROOT_SRC) \
	-I$(ROOT_LIB) \
	-I.

LIBS = \
	$(ROOT_LIB)/utf8proc/libutf8proc.a \

TYPE = c

all: $(TARGETS)
	@echo "All tests built"

u8s-basic: u8s-basic.c
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) 
	@echo "Built $@"

lastfm-source: lastfm-source.c
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) 
	@echo "Built $@"

clean:
	rm -rf $(TARGETS)

.PHONY: all clean

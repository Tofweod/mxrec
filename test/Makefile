TARGETS = $(patsubst %.c, %, $(wildcard *.c))

ROOT_SRC = ../src
ROOT_LIB = ../lib
OBJS = $(patsubst ../src/%.c, ../obj/%.o, $(shell find ../src -name "*.c"))
LIB_OBJS = $(filter-out ../obj/main.o, $(OBJS))
LIB_DIRS += -L$(ROOT_LIB)/utf8proc -L$(ROOT_LIB)/iniparser -L$(ROOT_LIB)/curl-impersonate -L$(ROOT_LIB)/yyjson/build
LD_FLAGS += -L$(LIB_DIRS) -Wl,-rpath,$(ROOT_LIB)/curl-impersonate/
LD_LIBS = -lcurl-impersonate-chrome -lm

CC = gcc
CFLAGS = -g -Wall

INCLUDES = \
	-I$(ROOT_SRC) \
	-I$(ROOT_LIB) \
	-I.

LIBS = \
	$(ROOT_LIB)/utf8proc/libutf8proc.a \
	$(ROOT_LIB)/iniparser/libiniparser.a \
	$(ROOT_LIB)/yyjson/build/libyyjson.a \
	$(ROOT_LIB)/libb64/src/libb64.a

TYPE = c

all: $(TARGETS)
	@echo "All tests built"

u8s-basic: u8s-basic.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

lastfm-source: lastfm-source.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

util-string: util-string.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

b64-qr: b64-qr.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

clean:
	rm -rf $(TARGETS)

.PHONY: all clean

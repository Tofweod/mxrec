TARGETS = $(patsubst %.c, %, $(wildcard *.c))

ROOT_SRC = ../src
ROOT_LIB = ../lib
OBJS = $(patsubst ../src/%.c, ../obj/%.o, $(shell find ../src -name "*.c"))
LIB_OBJS = $(filter-out ../obj/main.o, $(OBJS))
LIB_DIRS += -L$(ROOT_LIB)/utf8proc -L$(ROOT_LIB)/iniparser -L$(ROOT_LIB)/curl-impersonate -L$(ROOT_LIB)/yyjson/build -L$(ROOT_LIB)/libqrencode
LD_FLAGS += -L$(LIB_DIRS) -Wl,-rpath,$(ROOT_LIB)/curl-impersonate/
LD_LIBS = -lcurl-impersonate-chrome -lm

CC = gcc
CFLAGS = -g3 -Wall -fno-omit-frame-pointer
CXX = g++
CXXFLAGS = -std=c++17 -g3 -Wall -fno-omit-frame-pointer

INCLUDES = \
	-I$(ROOT_SRC) \
	-I$(ROOT_LIB) \
	-I$(ROOT_LIB)/indicators/single_include \
	-I.

LIBS = \
	$(ROOT_LIB)/utf8proc/libutf8proc.a \
	$(ROOT_LIB)/iniparser/libiniparser.a \
	$(ROOT_LIB)/yyjson/build/libyyjson.a \
	$(ROOT_LIB)/libb64/src/libb64.a \
	$(ROOT_LIB)/libqrencode/libqrencode.a \

TYPE = c

all: $(TARGETS)
	@echo "All tests built"

u8s-basic: u8s-basic.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

lastfm-web-source: lastfm-web-source.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

lastfm-api-source: lastfm-api-source.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

ncm-source: ncm-source.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

util-string: util-string.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

qr-test: qr-test.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

da-test: da-test.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

random-test: random-test.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

time-test: time-test.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(INCLUDES) $(LIBS) $(LD_FLAGS) $(LD_LIBS)
	@echo "Built $@"

progress-test: progress-test.cpp ../obj/./progress.o
	$(CXX) $(CXXFLAGS) $< ../obj/./progress.o -o $@ $(INCLUDES) -lpthread
	@echo "Built $@"

clean:
	rm -rf $(TARGETS)

.PHONY: all clean

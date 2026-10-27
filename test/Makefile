DEDUP_TEST_NAMES = track-key-test playlist-unique-test source-unique-test sample-test merge-test \
	lastfm-api-unique-test ncm-unique-test
TARGETS = $(filter-out $(DEDUP_TEST_NAMES),$(patsubst %.c, %, $(wildcard *.c)))

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

all: $(TARGETS) check-dedup check-dedup-memory
	@echo "All tests built and deduplication checks passed"

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
	rm -rf $(DEDUP_TEST_DIR)

DEDUP_TEST_DIR ?= .dedup-build
DEDUP_CFLAGS ?= -g3 -Wall
DEDUP_EXTRA_SOURCES ?=
DEDUP_LDFLAGS ?=
DEDUP_SOURCES = $(addprefix $(ROOT_SRC)/, assert.c xmalloc.c u8string.c artist.c track.c playlist.c \
	utils/file.c utils/normalize.c track-key.c random.c sample.c al.c) $(wildcard $(ROOT_SRC)/al/*.c) $(DEDUP_EXTRA_SOURCES)
DEDUP_HEADERS = $(wildcard $(ROOT_SRC)/*.h $(ROOT_SRC)/utils/*.h $(ROOT_SRC)/al/*.h) dedup-helpers.h
DEDUP_BINS = $(addprefix $(DEDUP_TEST_DIR)/,$(DEDUP_TEST_NAMES))
SOURCE_UNIQUE_BINS = $(addprefix $(DEDUP_TEST_DIR)/,lastfm-api-unique-test ncm-unique-test)

# Include the real source implementations while discarding unused network paths.
$(SOURCE_UNIQUE_BINS): SOURCE_UNIQUE_CFLAGS = -ffunction-sections -fdata-sections
$(SOURCE_UNIQUE_BINS): SOURCE_UNIQUE_LDFLAGS = -Wl,--gc-sections
$(DEDUP_TEST_DIR)/lastfm-api-unique-test: SOURCE_UNIQUE_LDFLAGS += -Wl,--wrap=track_new,--wrap=artist_new
$(DEDUP_TEST_DIR)/lastfm-api-unique-test: $(ROOT_SRC)/source/lastfm-api.c
$(DEDUP_TEST_DIR)/ncm-unique-test: DEDUP_SOURCES += $(ROOT_SRC)/json.c
$(DEDUP_TEST_DIR)/ncm-unique-test: SOURCE_UNIQUE_LDFLAGS += $(ROOT_LIB)/yyjson/build/libyyjson.a
$(DEDUP_TEST_DIR)/ncm-unique-test: $(ROOT_SRC)/source/ncm.c $(ROOT_SRC)/json.c $(ROOT_LIB)/yyjson/build/libyyjson.a

$(ROOT_LIB)/utf8proc/libutf8proc.a:
	$(MAKE) -C $(ROOT_LIB)/utf8proc

$(ROOT_LIB)/yyjson/build/libyyjson.a:
	$(MAKE) -C .. lib/yyjson/build/libyyjson.a

$(DEDUP_BINS): $(DEDUP_TEST_DIR)/%: %.c $(DEDUP_SOURCES) $(DEDUP_HEADERS) $(ROOT_LIB)/utf8proc/libutf8proc.a
	@mkdir -p $(@D)
	$(CC) $(DEDUP_CFLAGS) $(SOURCE_UNIQUE_CFLAGS) $(INCLUDES) $< $(DEDUP_SOURCES) $(ROOT_LIB)/utf8proc/libutf8proc.a -lm \
		$(if $(filter sample-test merge-test,$*),-Wl$(comma)--wrap=uniform01) $(SOURCE_UNIQUE_LDFLAGS) $(DEDUP_LDFLAGS) -o $@

comma := ,
check-dedup: $(DEDUP_BINS)
	@set -e; for binary in $(DEDUP_BINS); do "$$binary"; done

check-dedup-memory:
	$(MAKE) check-dedup DEDUP_TEST_DIR=$(DEDUP_TEST_DIR)/memory \
		DEDUP_EXTRA_SOURCES=support/allocation-check.c \
		DEDUP_LDFLAGS='-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=reallocarray,--wrap=strdup,--wrap=strndup,--wrap=free'

.PHONY: all clean check-dedup check-dedup-memory

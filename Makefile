TARGET = mxrec

SRC_DIR = src
SRC_SUBDIR += . al ex source utils
INCLUDE_DIR += lib src
OBJ_DIR = obj

LIB_DIR = lib
LIB_SUBDIR += . utf8proc iniparser libb64

MAKE = make
CMAKE = cmake
CC = gcc
C_FLAGS = -g3 -Wall -fno-omit-frame-pointer
LD = $(CC)
LIB_TYPE = a
INCLUDES += $(addprefix -I,$(INCLUDE_DIR))
LIB_DIRS += -Llib/utf8proc -Llib/iniparser -Llib/curl-impersonate -Llib/yyjson/build
LD_FLAGS += $(LIB_DIRS) -Wl,-rpath,lib/curl-impersonate/
LD_LIBS = -lcurl-impersonate-chrome -lm 

ifeq ($(CC), g++)
	TYPE = cpp
else
	TYPE = c
endif

SRCS += ${foreach subdir, $(SRC_SUBDIR), ${wildcard $(SRC_DIR)/$(subdir)/*.$(TYPE)}}
OBJS += $(patsubst $(SRC_DIR)/%.$(TYPE), $(OBJ_DIR)/%.o, $(SRCS))
jsondir=lib/yyjson
LIBS = \
	lib/utf8proc/libutf8proc.$(LIB_TYPE) \
	lib/iniparser/libiniparser.$(LIB_TYPE) \
	$(jsondir)/build/libyyjson.$(LIB_TYPE) \
	lib/libb64/src/libb64.a \
	lib/libqrencode/libqrencode.a


vpath %.$(TYPE) $(sort $(dir $(SRCS)))

all : $(TARGET) test
	@echo "Builded target:" $^
	@echo "Done"

# libs
lib/utf8proc/libutf8proc.$(LIB_TYPE):
	@echo "Building lib for utf8proc..."
	$(MAKE) -C lib/utf8proc

lib/iniparser/libiniparser.$(LIB_TYPE):
	@echo "Building lib for iniparser..."
	$(CMAKE) -S lib/iniparser -B lib/iniparser
	$(MAKE) -C lib/iniparser

lib/libb64/src/libb64.$(LIB_TYPE):
	@echo "Building lib for libb64..."
	$(MAKE) -C lib/libb64

$(jsondir)/build/libyyjson.$(LIB_TYPE):
	@echo "Building lib for yyjson..."
	mkdir -p $(jsondir)/build
	$(CMAKE) -S $(jsondir) -B $(jsondir)/build
	$(MAKE) -C $(jsondir)/build

lib/libqrencode/libqrencode.a:
	@echo "Building lib for libqrencode..."
	cd lib/libqrencode && ./autogen.sh && ./configure --disable-shared --enable-static --without-tools
	$(MAKE) -C lib/libqrencode
	cd lib/libqrencode && cp .libs/libqrencode.a libqrencode.a

$(TARGET) : $(OBJS) $(LIBS)
	@mkdir -p $(@D)
	@echo "Linking" $@ "from" $^ "..."
	$(LD) -o $@ $^ $(LD_FLAGS) $(LD_LIBS)
	@echo "Link finished\n"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.$(TYPE) $(LIBS)
	@mkdir -p $(dir $@)
	@mkdir -p $(@D)
	@echo "Compiling" $@ "from" $< "..."
	$(CC) -MMD -MP -c -o $@ $< $(C_FLAGS) $(INCLUDES)

test: $(OBJS)
	@echo "Start building tests\n"
	$(MAKE) -C test

DEP = $(OBJS:.o=.d)

-include $(DEP)

.PHONY : clean cleanobj test
clean : cleanobj
	@echo "Remove all executable files"
	rm -f $(TARGET)
	make -C lib/iniparser clean
	make -C lib/utf8proc clean
	make -C lib/libb64 clean
	rm -rf $(jsondir)/build
	make -C lib/libqrencode clean
	rm -r lib/libqrencode/libqrencode.a
	make -C test clean

cleanobj :
	@echo "Remove object files"
	rm -rf $(OBJ_DIR)

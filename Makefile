TARGET = mxrec

SRC_DIR = src
SRC_SUBDIR += . al ex source utils
INCLUDE_DIR += lib src lib/indicators/single_include
OBJ_DIR = obj

LIB_DIR = lib
LIB_SUBDIR += . utf8proc iniparser libb64

MAKE = make
CMAKE = cmake
CC = gcc
C_FLAGS = -g3 -Wall -fno-omit-frame-pointer -DNCM_DEFAULT_WORK_DIR='"$(NCM_DEFAULT_WORK_DIR)"'
CXX = g++
CXX_FLAGS = -std=c++17 -g3 -Wall -fno-omit-frame-pointer
LD = $(CXX)
LIB_TYPE = a
INCLUDES += $(addprefix -I,$(INCLUDE_DIR))
LIB_DIRS += -Llib/utf8proc -Llib/iniparser -Llib/curl-impersonate -Llib/yyjson/build
LD_FLAGS += $(LIB_DIRS) -Wl,-rpath,'$$ORIGIN/lib/curl-impersonate/'
LD_LIBS = -lcurl-impersonate-chrome -lm -lstdc++ -lpthread

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/mxrec
NCMDIR ?= $(DATADIR)/netease
CURLLIBDIR ?= $(PREFIX)/lib/mxrec/curl-impersonate
PATCHELF ?= patchelf
NPM ?= npm
NCM_STAMP = lib/netease/.node_modules.stamp
NCM_DEFAULT_WORK_DIR ?= $(NCMDIR)

SRCS_C += ${foreach subdir, $(SRC_SUBDIR), ${wildcard $(SRC_DIR)/$(subdir)/*.c}}
SRCS_CPP += ${foreach subdir, $(SRC_SUBDIR), ${wildcard $(SRC_DIR)/$(subdir)/*.cpp}}
SRCS += $(SRCS_C) $(SRCS_CPP)
OBJS += $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_C))
OBJS += $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS_CPP))
jsondir=lib/yyjson
LIBS = \
	lib/utf8proc/libutf8proc.$(LIB_TYPE) \
	lib/iniparser/libiniparser.$(LIB_TYPE) \
	$(jsondir)/build/libyyjson.$(LIB_TYPE) \
	lib/libb64/src/libb64.a \
	lib/libqrencode/libqrencode.a


vpath %.c $(sort $(dir $(SRCS_C)))
vpath %.cpp $(sort $(dir $(SRCS_CPP)))

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

lib/indicators/single_include/indicators/indicators.cpp:
	@echo "Building lib for lib indicators..."
	cd lib/indicators && python3 utils/amalgamate/amalgamate.py -c single_include.json -s . 

$(NCM_STAMP): lib/netease/package.json lib/netease/package-lock.json
	$(NPM) --prefix lib/netease install
	@touch $@

$(TARGET) : $(OBJS) $(LIBS) $(NCM_STAMP)
	@mkdir -p $(@D)
	@echo "Linking" $@ "from" $^ "..."
	$(LD) -o $@ $^ $(LD_FLAGS) $(LD_LIBS)
	@echo "Link finished\n"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(LIBS)
	@mkdir -p $(dir $@)
	@echo "Compiling" $@ "from" $< "..."
	$(CC) -MMD -MP -c -o $@ $< $(C_FLAGS) $(INCLUDES)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(LIBS)
	@mkdir -p $(dir $@)
	@echo "Compiling" $@ "from" $< "..."
	$(CXX) -MMD -MP -c -o $@ $< $(CXX_FLAGS) $(INCLUDES)

test: $(OBJS)
	@echo "Start building tests\n"
	$(MAKE) -C test

DEP = $(OBJS:.o=.d)

-include $(DEP)

.PHONY : clean cleanobj test install uninstall
install: $(TARGET)
	@command -v $(PATCHELF) >/dev/null 2>&1 || { echo "patchelf is required for install"; exit 1; }
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR) $(DESTDIR)$(NCMDIR) $(DESTDIR)$(CURLLIBDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -a lib/netease/. $(DESTDIR)$(NCMDIR)/
	cp -a lib/curl-impersonate/. $(DESTDIR)$(CURLLIBDIR)/
	$(PATCHELF) --set-rpath $(CURLLIBDIR) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 0644 example.ini $(DESTDIR)$(DATADIR)/config.ini

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(NCMDIR)
	rm -rf $(DESTDIR)$(CURLLIBDIR)
	rm -rf $(DESTDIR)$(DATADIR)
	rm -rf $(DESTDIR)$(PREFIX)/lib/mxrec

clean : cleanobj
	@echo "Remove all executable files"
	rm -f $(TARGET) $(NCM_STAMP)
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

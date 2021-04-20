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

BIN=../bin/ticksvc-$(TARGET)
CFLAGS=-W -Wall -fdata-sections -ffunction-sections -Wl,--gc-sections

#
# Uncomment to disable features at compile time.
# The descriptions of each flag are detailed in src/common.h.
#
# I recommend to leave these alone unless you really know what you're doing.
#

#CFLAGS := $(CFLAGS) -DTICK_VERBOSE=0

#CFLAGS := $(CFLAGS) -DTICK_FEATURES_CRYPTO=0
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_DNS=0
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_EXEC=0
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_FILE=0
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_SHELL=0
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_PIVOT=0

#CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0
#CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_ENV=0
#CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_ARGV=0

#CFLAGS := $(CFLAGS) -DTICK_CONFIG_HOSTNAME=127.0.0.1

##############################################################################
# Platform dependent configuration.

# Debug compilation for the local platform.
# This is the default. Keeps the debugging symbols and forces verbose output.
ifeq ($(TARGET),debug)
CC=gcc
ST=@true
CFLAGS := $(filter-out -DTICK_VERBOSE=0,$(CFLAGS))
CFLAGS := $(CFLAGS) -g -DTICK_VERBOSE=1

#----------------------------------------------------------------------------#

# Generic Android on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-android)
FLAVOR=arm-linux-androideabi
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

# Generic Android on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-android)
FLAVOR=aarch64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm64/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

# Generic Android on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-android)
FLAVOR=mipsel-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

# Generic Android on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-android)
FLAVOR=mips64el-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips64/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

# Generic Android on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-android)
FLAVOR=i686-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

# Generic Android on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-android)
FLAVOR=x86_64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86_64/
CFLAGS := $(filter-out -DTICK_CONFIG_USE_FILE=1,$(CFLAGS))
CFLAGS := $(CFLAGS) -DTICK_CONFIG_USE_FILE=0

#----------------------------------------------------------------------------#

# Generic Linux on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-linux)
FLAVOR=arm-linux-musleabi
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static

# Generic Linux on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-linux)
FLAVOR=aarch64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static

# Generic Linux on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-linux)
FLAVOR=mips-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static

# Generic Linux on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-linux)
FLAVOR=mips64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static

# Generic Linux on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-linux)
FLAVOR=i686-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -m32 -Os -fPIC -static

# Generic Linux on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-linux)
FLAVOR=x86_64-linux-musl
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -m64 -Os -fPIC -static

#----------------------------------------------------------------------------#

# Generic Windows on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-windows)
FLAVOR=i686-w64-mingw32
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -Wno-unknown-pragmas -mwindows -static -lmingw32 -lwsock32 -lws2_32
BIN := $(BIN).exe

# Generic Windows on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-windows)
FLAVOR=x86_64-w64-mingw32
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -Wno-unknown-pragmas -mwindows -static -lmingw32 -lwsock32 -lws2_32
BIN := $(BIN).exe

#----------------------------------------------------------------------------#

# Fail if the TARGET value is unknown.
else
$(error unknown value for TARGET: $(TARGET))
endif

##############################################################################
# Source and object files.

# Add all .c files in the root folder.
SRC=$(wildcard *.c)

# If SSL is disabled, remove the ssl.c module.
# If SSL is enabled, add the BearSSL source code and includes.
# We need this because the compiler won't always be removing the unused code
# by itself, even when we explicitly told it to. (I'm looking at you, mingw)
ifneq ($(filter -DTICK_FEATURES_CRYPTO=0,$(CFLAGS)),)
	SRC := $(filter-out ssl.c,$(SRC))
else
	SRC += $(wildcard BearSSL/src/*.c)
	SRC += $(wildcard BearSSL/src/*/*.c)
	CFLAGS := -I./BearSSL/src -I./BearSSL/inc $(CFLAGS)
endif

# Remove other modules based on the feature list too.
ifneq ($(filter -DTICK_FEATURES_DNS=0,$(CFLAGS)),)
	SRC := $(filter-out dns.c,$(SRC))
endif
ifneq ($(filter -DTICK_FEATURES_FILE=0,$(CFLAGS)),)
	SRC := $(filter-out file.c,$(SRC))
endif
ifneq ($(filter -DTICK_FEATURES_SHELL=0,$(CFLAGS)),)
	SRC := $(filter-out shell.c,$(SRC))
endif
ifneq ($(filter -DTICK_FEATURES_PIVOT=0,$(CFLAGS)),)
	SRC := $(filter-out pivot.c,$(SRC))
endif

# Derive the names of object files from source files.
OBJ=$(patsubst %.c,%.o,$(SRC))

##############################################################################
# Recipes.

# Quick "optimization" note.
# I tried here, instead of compiling every .o separately and then linking,
# to give gcc all of the source files in one go and just generate the binary.
# This resulted in *much* slower compile times... and literally no benefit!
# DISAPPOINTED! <kevinsorbo.gif>

.PHONY: all clean

all:
	@echo Building for platform: $(TARGET)
	@$(MAKE) --no-print-directory $(BIN)
	@echo Finished building for platform: $(TARGET)

%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

#../bin/ticksvc-%: $(SRC)
../bin/ticksvc-%: $(OBJ)
	mkdir -p -- $$(dirname $@)
#	$(CC) $(SRC) $(CFLAGS) -o $@
	$(CC) $(OBJ) $(CFLAGS) -o $@
	$(ST) $@

clean:
	rm -f -- $(OBJ) $(BIN)

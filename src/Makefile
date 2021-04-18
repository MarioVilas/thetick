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
CFLAGS=-W -Wall

# Uncomment to disable features at compile time.
#
# TICK_FEATURES_NO_CRYPTO: disables protocol encryption
# TICK_FEATURES_NO_SHELL:  disables the shell command
# TICK_FEATURES_NO_EXEC:   disables the exec command
# TICK_FEATURES_NO_FILE:   disables push, pop, rm and chmod commands
# TICK_FEATURES_NO_DNS:    disables DNS resolution (affects dig and proxy, and
#                          you can only use IP addresses for setup)
# TICK_FEATURES_NO_PIVOT:  disables pivoting and proxy support
#
# Most of these have little impact in the resulting binary size, except for
# SSL support, which bloats the binary a lot. I'm working on it...

#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_CRYPTO
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_SHELL
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_EXEC
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_FILE
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_DNS
#CFLAGS := $(CFLAGS) -DTICK_FEATURES_NO_PIVOT

##############################################################################
# Platform dependent configuration.

# Debug compilation for the local platform.
# This is the default. Keeps the debugging symbols and enables verbose output.
ifeq ($(TARGET),debug)
CC=gcc
ST=@true
CFLAGS := $(CFLAGS) -g -DTICK_VERBOSE

#----------------------------------------------------------------------------#

# Generic Android on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-android)
FLAVOR=arm-linux-androideabi
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm/

# Generic Android on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-android)
FLAVOR=aarch64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm64/

# Generic Android on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-android)
FLAVOR=mipsel-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips/

# Generic Android on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-android)
FLAVOR=mips64el-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips64/

# Generic Android on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-android)
FLAVOR=i686-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86/

# Generic Android on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-android)
FLAVOR=x86_64-linux-android
CC=$(FLAVOR)-gcc
ST=$(FLAVOR)-strip
CFLAGS := $(CFLAGS) -Os -fPIC -static -fdiagnostics-color=always --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86_64/

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
# If SSL is enabled, add the BearSSL source code.
# (Turns out gcc's linker isn't so smart after all...)
ifneq ($(filter -DTICK_FEATURES_NO_CRYPTO,$(CFLAGS)),)
	SRC := $(filter-out ssl.c,$(SRC))
else
	SRC += $(wildcard BearSSL/src/*.c)
	SRC += $(wildcard BearSSL/src/*/*.c)
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
	$(CC) -c $< -I./BearSSL/src -I./BearSSL/inc $(CFLAGS) -o $@

#../bin/ticksvc-%: $(SRC)
../bin/ticksvc-%: $(OBJ)
	mkdir -p -- $$(dirname $@)
#	$(CC) -I./BearSSL/src -I./BearSSL/inc $(SRC) $(CFLAGS) -o $@
	$(CC) $(OBJ) $(CFLAGS) -o $@
	$(ST) $@

clean:
	rm -f -- $(OBJ) $(BIN)

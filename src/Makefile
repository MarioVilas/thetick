#
# The Tick, a Linux embedded backdoor.
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
# No specific targets are (currently) supported. Stay tuned!

TARGET := $(or $(TARGET),debug)

##############################################################################
# Source, object and binary files.

SRC=$(wildcard *.c)
ASM=$(patsubst %.c,%.s,$(SRC))
OBJ=$(patsubst %.c,%.o,$(SRC))
BIN=../bin/ticksvc-$(TARGET)

##############################################################################
# Platform dependent configuration.

# Debug compilation for the local platform.
# This is the default. Keeps the debugging symbols and enables verbose output.
ifeq ($(TARGET),debug)
CC=gcc
AS=as
ST=@true
CPPFLAGS=-g -DTICK_CONFIG_VERBOSE
LDDFLAGS=

# Generic Android on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-android)
CC=i686-linux-android-gcc
AS=i686-linux-android-as
ST=i686-linux-android-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86/

# Generic Android on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-android)
CC=x86_64-linux-android-gcc
AS=x86_64-linux-android-as
ST=x86_64-linux-android-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86_64/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-x86_64/

# Generic Android on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-android)
CC=arm-linux-androideabi-gcc
AS=arm-linux-androideabi-as
ST=arm-linux-androideabi-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm/

# Generic Android on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-android)
CC=aarch64-linux-android-gcc
AS=aarch64-linux-android-as
ST=aarch64-linux-android-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm64/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-arm64/

# Generic Android on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-android)
CC=mipsel-linux-android-gcc
AS=mipsel-linux-android-as
ST=mipsel-linux-android-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips/

# Generic Android on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-android)
CC=mips64el-linux-android-gcc
AS=mips64el-linux-android-as
ST=mips64el-linux-android-strip
CPPFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips64/
LDDFLAGS=-fPIC -fPIE -pie --sysroot=/usr/lib/android-ndk/platforms/android-21/arch-mips64/

# Generic Linux on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-linux)
CC=arm-linux-gnueabi-gcc
AS=arm-linux-gnueabi-as
ST=arm-linux-gnueabi-strip
CPPFLAGS=-fPIC -fPIE -pie
LDDFLAGS=-fPIC -fPIE -pie

# Generic Linux on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm64-linux)
CC=aarch64-linux-gnu-gcc
AS=aarch64-linux-gnu-as
ST=aarch64-linux-gnu-strip
CPPFLAGS=-fPIC -fPIE -pie
LDDFLAGS=-fPIC -fPIE -pie

# Generic Linux on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-linux)
CC=mips-linux-gnu-gcc
AS=mips-linux-gnu-as
ST=mips-linux-gnu-strip
CPPFLAGS=-fPIC -fPIE -pie
LDDFLAGS=-fPIC -fPIE -pie

# Generic Linux on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips64-linux)
CC=mips64-linux-gnuabi64-gcc
AS=mips64-linux-gnuabi64-as
ST=mips64-linux-gnuabi64-strip
CPPFLAGS=-fPIC -fPIE -pie
LDDFLAGS=-fPIC -fPIE -pie

# Generic Linux on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-x86-linux)
CC=gcc
AS=as
ST=strip
CPPFLAGS=-m32 -fPIC -fPIE -pie
LDDFLAGS=-m32 -fPIC -fPIE -pie

# Generic Linux on 64 bit Intel platforms.
else ifeq ($(TARGET),generic-x86_64-linux)
CC=gcc
AS=as
ST=strip
CPPFLAGS=-m64 -fPIC -fPIE -pie
LDDFLAGS=-m64 -fPIC -fPIE -pie

#----------------------------------------------------------------------------#

# Add more platforms like this:
#else ifeq ($(TARGET),example)
#CC=example-gcc
#AS=example-as
#ST=example-strip
#CPPFLAGS=-flags
#LDDFLAGS=-flags

# Fail if the TARGET value is unknown.
else
$(error unknown value for TARGET: $(TARGET))
endif

##############################################################################
# Make targets.

.PHONY: all clean

all:
	@echo Building for platform: $(TARGET)
	@$(MAKE) --no-print-directory $(BIN)
	@echo Finished building for platform: $(TARGET)

%.s: %.c
	$(CC) -S $< $(CPPFLAGS) -o $@

%.o: %.s
	$(AS) $< -o $@

../bin/ticksvc-%: $(OBJ)
	mkdir -p -- $$(dirname $@)
	$(CC) $(OBJ) $(LDDFLAGS) -o $@
	$(ST) $@

clean:
	rm -f -- $(ASM) $(OBJ) $(BIN)

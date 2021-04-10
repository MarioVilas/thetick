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
# where the only assumption is there is a Linux platform and you choose the
# architecture and ABI. The other is for specific platforms, where some kind
# of tweaking is needed to get things working.
#
# Generic targets currently are:
#
# - generic-arm-32
# - generic-arm-64
# - generic-intel-32
# - generic-intel-64
# - generic-mips-32
# - generic-mips-64
#
# No specific targets are currently supported.

TARGET := $(or $(TARGET),debug)

##############################################################################
# Source, object and binary files.

SRC=$(wildcard *.c)
ASM=$(patsubst %.c,%.s,$(SRC))
OBJ=$(patsubst %.c,%.o,$(SRC))
BIN=../bin/ticksvc-$(TARGET)

##############################################################################
# Platform dependent configuration.

# Generic Linux on 64 bit Intel platforms.
ifeq ($(TARGET),generic-intel-64)
CC=gcc
AS=as
ST=strip
CPPFLAGS=-m64 -pie -fPIC
LDDFLAGS=-m64 -pie -fPIC

# Generic Linux on 32 bit Intel platforms.
else ifeq ($(TARGET),generic-intel-32)
CC=gcc
AS=as
ST=strip
CPPFLAGS=-m32 -pie -fPIC
LDDFLAGS=-m32 -pie -fPIC

# Generic Linux on 32 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-32)
CC=arm-linux-gnueabi-gcc
AS=arm-linux-gnueabi-as
ST=arm-linux-gnueabi-strip
CPPFLAGS=-pie -fPIC
LDDFLAGS=-pie -fPIC

# Generic Linux on 64 bit ARM platforms.
else ifeq ($(TARGET),generic-arm-64)
CC=aarch64-linux-gnu-gcc
AS=aarch64-linux-gnu-as
ST=aarch64-linux-gnu-strip
CPPFLAGS=-pie -fPIC
LDDFLAGS=-pie -fPIC

# Generic Linux on 32 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-32)
CC=mips-linux-gnu-gcc
AS=mips-linux-gnu-as
ST=mips-linux-gnu-strip
CPPFLAGS=-pie -fPIC
LDDFLAGS=-pie -fPIC

# Generic Linux on 64 bit MIPS platforms.
else ifeq ($(TARGET),generic-mips-64)
CC=mips64-linux-gnuabi64-gcc
AS=mips64-linux-gnuabi64-as
ST=mips64-linux-gnuabi64-strip
CPPFLAGS=-pie -fPIC
LDDFLAGS=-pie -fPIC

#----------------------------------------------------------------------------#

# Add more platforms like this:
#else ifeq ($(TARGET),example)
#CC=example-gcc
#AS=example-as
#ST=example-strip
#CPPFLAGS=-flags
#LDDFLAGS=-flags

# Debug compilation for the local platform.
# This is the default and it keeps the debugging symbols on.
else ifeq ($(TARGET),debug)
CC=gcc
AS=as
ST=@true
CPPFLAGS=-g
LDDFLAGS=

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

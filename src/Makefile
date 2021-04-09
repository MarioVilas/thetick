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
# Other valid values are:
#
# - lexmark-cx310dn

TARGET := $(or $(TARGET),debug)

##############################################################################
# Source, object and binary files.

SRC=$(wildcard *.c)
ASM=$(patsubst %.c,%.s,$(SRC))
OBJ=$(patsubst %.c,%.o,$(SRC))
BIN=../bin/ticksvc-$(TARGET)

##############################################################################
# Platform dependent configuration.

# Cross-compile for the Lexmark CX310DN printer.
ifeq ($(TARGET),lexmark-cx310dn)
CC=arm-linux-gnueabi-gcc
AS=arm-linux-gnueabi-as -meabi=4
ST=arm-linux-gnueabi-strip
CPPFLAGS=
LDDFLAGS=

# Add more platforms like this:
#else ifeq ($(TARGET),"example")
#CC=example-gcc
#AS=example-as
#ST=example-strip
#CPPFLAGS=
#LDDFLAGS=

# Debug compilation for the local platform.
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

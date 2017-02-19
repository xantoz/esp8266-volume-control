# Makefile for compiling python files to *.mpy to save space etc.
# Assumes you have mpy-cross in PATH.

all: mcp42xxx.mpy volume_control.mpy

%.mpy: %.py
	mpy-cross -o '$@' '$^'

clean:
	rm -f *.mpy

#!/bin/bash
# By Peter PT_ Tillema

# Building for the calculator
make

# Building for the computer
make -f makefile.computer

# Build the hooks
fasmg hooks/hooks.asm bin/ICEAPPV.8xv
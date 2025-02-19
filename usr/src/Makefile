################################################################################
##                                                                            ##
##                     Copyright (c) 2012-2025 James Card                     ##
##                                                                            ##
## Permission is hereby granted, free of charge, to any person obtaining a    ##
## copy of this software and associated documentation files (the "Software"), ##
## to deal in the Software without restriction, including without limitation  ##
## the rights to use, copy, modify, merge, publish, distribute, sublicense,   ##
## and/or sell copies of the Software, and to permit persons to whom the      ##
## Software is furnished to do so, subject to the following conditions:       ##
##                                                                            ##
## The above copyright notice and this permission notice shall be included    ##
## in all copies or substantial portions of the Software.                     ##
##                                                                            ##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR ##
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   ##
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    ##
## THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER ##
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    ##
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        ##
## DEALINGS IN THE SOFTWARE.                                                  ##
##                                                                            ##
##                                 James Card                                 ##
##                          http://www.jamescard.org                          ##
##                                                                            ##
################################################################################

all:
	riscv32-unknown-elf-gcc -I../.. -Wall -Wextra -pedantic -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -Ttext=0x1000 -e _start -o ../obj/init.elf init.c
	riscv32-unknown-elf-objcopy -O binary ../obj/init.elf ../obj/init.bin
	riscv32-unknown-elf-objcopy -O binary --only-section=.text ../obj/init.elf ../obj/init.text
	cp ../obj/init.bin ../bin/init
	../bin/addNanoOsExeMetadata ../bin/init ../obj/init.text


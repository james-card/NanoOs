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

CC := riscv32-unknown-elf-gcc
OBJCOPY := riscv32-unknown-elf-objcopy
RM := rm -f

WARNINGS := -Wall -Wextra -pedantic -Werror
FLAGS := -march=rv32im -mabi=ilp32 -nostdlib -nodefaultlibs -fno-builtin \
    -ffreestanding -nostartfiles -Ttext=0x1000 \

BIN_DIR := ../bin
OBJ_DIR := ../obj

INCLUDES := \
    -I../.. \
    -I../include \

WARNINGS := \
    -Wno-unused-variable \

OBJ_FILES := \
    $(OBJ_DIR)/start.o \
    $(OBJ_DIR)/div64.o \

LINKS := \
    -Wl,-e_start \

PROGRAMS := \
    $(BIN_DIR)/init \

all: $(PROGRAMS)

$(OBJ_DIR)/%.o: %.c
	$(CC) $(INCLUDES) $(WARNINGS) $(FLAGS) -c $< -o $@

$(OBJ_DIR)/%.elf: $(OBJ_DIR)/%.o
	$(CC) $(INCLUDES) $(WARNINGS) $(FLAGS) $(OBJ_FILES) $< $(LINKS) -o $@

$(OBJ_DIR)/%.bin: $(OBJ_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@

$(OBJ_DIR)/%.text: $(OBJ_DIR)/%.elf
	$(OBJCOPY) -O binary --only-section=.text $< $@

$(BIN_DIR)/%: $(OBJ_DIR)/%.bin $(OBJ_DIR)/%.text
	cp $(OBJ_DIR)/$*.bin $@
	$(BIN_DIR)/addNanoOsExeMetadata $@ $(OBJ_DIR)/$*.text

clean:
	$(RM) $(OBJ_DIR)/*.text $(OBJ_DIR)/*.bin $(OBJ_DIR)/*.elf $(OBJ_DIR)/*.o


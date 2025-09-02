# Makefile for NanoOS Overlays
# ARM Cortex-M0 target

OBJ_DIR = ../../../obj
BIN_DIR = ../../../bin

# CLI tools
MKDIR = mkdir -p
RM = rm -rf

# Build tools
CC = arm-none-eabi-gcc
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size

# Compiler flags
CFLAGS = -mcpu=cortex-m0 -Os -nostdlib -ffreestanding
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -ffunction-sections -fdata-sections

# Linker flags
LDFLAGS = -T ../../NanoOs.ld --gc-sections

# Source and target files
TARGET = helloworld
SOURCE = $(TARGET).c
BINARY = $(BIN_DIR)/overlay_$(TARGET).bin
ELF = $(OBJ_DIR)/$(TARGET).elf
OBJECT = $(OBJ_DIR)/$(TARGET).o

INCLUDES = \
    -I../../../.. \
    -I../../../include \

# Default target
all: $(BINARY)

# Build overlay binary
$(BINARY): $(ELF)
	@echo "Creating binary: $@"
	$(MKDIR) "$(BIN_DIR)"
	$(OBJCOPY) -O binary $< $@
	@echo "Binary size:"
	@ls -la $@

# Link ELF file
$(ELF): $(OBJECT)
	@echo "Linking: $@"
	$(MKDIR) "$(OBJ_DIR)"
	$(LD) $(LDFLAGS) $< -o $@
	$(SIZE) $@

# Compile object file
$(OBJECT): $(SOURCE)
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Generate disassembly for debugging
disasm: $(ELF)
	$(OBJDUMP) -d -S $< > $(TARGET).dis
	@echo "Disassembly written to $(TARGET).dis"

# Show section information
sections: $(ELF)
	$(OBJDUMP) -h $<

# Show symbol table
symbols: $(ELF)
	$(OBJDUMP) -t $<

# Clean build artifacts
clean:
	$(RM) $(OBJECT) $(ELF) $(BINARY) $(TARGET).dis

# Show help
help:
	@echo "NanoOS Overlay Build System"
	@echo "Usage:"
	@echo "  make          - Build overlay_hello.bin from hello.c"
	@echo "  make disasm   - Generate disassembly listing"
	@echo "  make sections - Show ELF section information"  
	@echo "  make symbols  - Show symbol table"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Include paths:"
	@arm-none-eabi-gcc $(CFLAGS) $(INCLUDES) -E -Wp,-v -x c /dev/null 2>&1 | grep "^ "

# Phony targets
.PHONY: all clean disasm sections symbols help

# Additional target for different source files
# Usage: make SOURCE=mycommand.c TARGET=mycommand
ifneq ($(SOURCE),hello.c)
$(warning Building $(TARGET) from $(SOURCE))
endif

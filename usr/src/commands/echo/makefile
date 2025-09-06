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
TARGET = echo
ELF = $(OBJ_DIR)/$(TARGET).elf
BINARY = $(BIN_DIR)/$(TARGET)

SOURCES = \
    ../../start.c \
    main.c \

OBJECTS = \
    $(OBJ_DIR)/start.o \
    $(OBJ_DIR)/main.o \
    $(OBJ_DIR)/OverlayMap.o \

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
$(ELF): $(OBJECTS)
	@echo "Linking: $@"
	$(MKDIR) "$(OBJ_DIR)"
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@
	$(SIZE) $@

# Compile object file
$(OBJ_DIR)/start.o: ../../start.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

OverlayMap.c: $(SOURCES)
	@echo "Creating OverlayMap.c"
	../../util/mkOverlayMap.sh $(SOURCES)

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
	$(RM) $(OBJECTS) $(ELF) $(BINARY) $(TARGET).dis OverlayMap.c

# Show help
help:
	@echo "NanoOS Overlay Build System"
	@echo "Usage:"
	@echo "  make          - Build $(TARGET)"
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


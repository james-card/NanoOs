# All paths are relative to the command directories
OBJ_DIR = ../../../obj
BIN_DIR = ../../../bin

# CLI tools
MKDIR := mkdir -p
RM    := rm -rf

# Build tools
ifeq ($(strip $(COMPILE)),)
    override COMPILE := arm-none-eabi-gcc
endif
ifeq ($(strip $(LINK)),)
    override LINK    := arm-none-eabi-ld
endif
ifeq ($(strip $(OBJCOPY)),)
    override OBJCOPY := arm-none-eabi-objcopy
endif
ifeq ($(strip $(OBJDUMP)),)
    override OBJDUMP := arm-none-eabi-objdump
endif
ifeq ($(strip $(SIZE)),)
    override SIZE    := arm-none-eabi-size
endif

ifeq ($(COMPILE),arm-none-eabi-gcc)
    CPU := -mcpu=cortex-m0
endif

# Linker flags
LDFLAGS = -T ../../NanoOs.ld --gc-sections

# Compiler flags
CFLAGS = $(CPU) -Os -nostdlib -ffreestanding
CFLAGS += -Wall -Wextra -std=c17
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-jump-tables

LINKS = \

INCLUDES = \
    -I../../../../src/kernel \
    -I../../../../src/user \
    -I../../../include \

WARNINGS = \
    -Wall \
    -Wextra \
    -pedantic \
    -Werror \

# Source and target files
ELF = $(OBJ_DIR)/$(TARGET).elf
BINARY = $(BIN_DIR)/$(TARGET)

SOURCES = \
    ../../start.c \
    main.c \

OBJECTS = \
    $(OBJ_DIR)/start.o \
    $(OBJ_DIR)/$(TARGET)main.o \
    $(OBJ_DIR)/$(TARGET)OverlayMap.o \

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
	$(LINK) $(LDFLAGS) $(OBJECTS) $(LINKS) -o $@
	$(SIZE) $@

# Compile object files
$(OBJ_DIR)/$(TARGET)main.o: main.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(COMPILE) $(WARNINGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/start.o: ../../start.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(COMPILE) $(WARNINGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(COMPILE) $(WARNINGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET)OverlayMap.c: $(SOURCES)
	@echo "Creating $(TARGET)OverlayMap.c"
	../../util/mkOverlayMap.sh $(TARGET)OverlayMap.c $(SOURCES)

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
	$(RM) $(OBJECTS) $(ELF) $(BINARY) $(TARGET).dis $(TARGET)OverlayMap.c

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


COMPONENTS := kernel user

OBJ_DIR = obj
BIN_DIR = bin

# CLI tools
MKDIR := mkdir -p
RM    := rm -rf

# Build tools
ifeq ($(strip $(COMPILE)),)
    override COMPILE := gcc
endif
ifeq ($(strip $(LINK)),)
    override LINK    := ld
endif
ifeq ($(strip $(OBJCOPY)),)
    override OBJCOPY := objcopy
endif
ifeq ($(strip $(OBJDUMP)),)
    override OBJDUMP := objdump
endif
ifeq ($(strip $(SIZE)),)
    override SIZE    := size
endif

ifeq ($(COMPILE),arm-none-eabi-gcc)
    CPU := -mcpu=cortex-m0
endif

# Linker flags
LDFLAGS = -T ../../NanoOs.ld --gc-sections

# Compiler flags
CFLAGS = $(CPU) -std=gnu17

LINKS = \

INCLUDES = \
    -Ikernel \
    -Iuser \

WARNINGS = \
    -Wall \
    -Wextra \
    -pedantic \
    -Werror \

# Source and target files
BINARY = $(BIN_DIR)/nano-os-sim

SOURCES = \

OBJECTS = \
    $(OBJ_DIR)/HalPosix.o \
    $(OBJ_DIR)/NanoOsSim.o \
    $(OBJ_DIR)/SdCardPosix.o \

# Default target
all: $(BINARY)

# Build overlay binary
$(BINARY): $(COMPONENTS) $(OBJECTS)
	@echo "Creating binary: $@"
	$(MKDIR) "$(BIN_DIR)"
	$(OBJCOPY) -O binary $< $@
	@echo "Binary size:"
	@ls -la $@

$(COMPONENTS): ./%:
	$(MAKE) -C $@ TARGET="$@" COMPILE=$(COMPILE) LINK=$(LINK) \
		OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) SIZE=$(SIZE)

# Compile object files
$(OBJ_DIR)/%.o: %.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(COMPILE) $(WARNINGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Generate disassembly for debugging
disasm: $(BINARY)
	$(OBJDUMP) -d -S $< > $(TARGET).dis
	@echo "Disassembly written to $(TARGET).dis"

# Show section information
sections: $(BINARY)
	$(OBJDUMP) -h $<

# Show symbol table
symbols: $(BINARY)
	$(OBJDUMP) -t $<

# Clean build artifacts
clean:
	$(RM) $(OBJECTS) $(BINARY) $(TARGET).dis $(TARGET)OverlayMap.c

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


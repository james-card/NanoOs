OBJ_DIR = ../obj
BIN_DIR = ../bin

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
LDFLAGS = 

# Compiler flags
CFLAGS = $(CPU) -std=c17

LINKS = \

INCLUDES = \
    -I../user \

WARNINGS = \
    -Wall \
    -Wextra \
    -pedantic \
    -Werror \

SOURCES = \

OBJECTS = \
    $(OBJ_DIR)/Commands.o \
    $(OBJ_DIR)/Console.o \
    $(OBJ_DIR)/Coroutines.o \
    $(OBJ_DIR)/ExFatFilesystem.o \
    $(OBJ_DIR)/ExFatProcess.o \
    $(OBJ_DIR)/Filesystem.o \
    $(OBJ_DIR)/Link.o \
    $(OBJ_DIR)/MemoryManager.o \
    $(OBJ_DIR)/Messages.o \
    $(OBJ_DIR)/NanoOs.o \
    $(OBJ_DIR)/NanoOsOverlay.o \
    $(OBJ_DIR)/Processes.o \
    $(OBJ_DIR)/Scheduler.o \
    $(OBJ_DIR)/SdCard.o \

# Default target
all: $(OBJECTS)

# Compile object files
$(OBJ_DIR)/%.o: %.c
	@echo "Compiling: $<"
	$(MKDIR) "$(OBJ_DIR)"
	$(COMPILE) $(WARNINGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	$(RM) $(OBJECTS)

# Phony targets
.PHONY: all clean


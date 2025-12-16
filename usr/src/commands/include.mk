# CLI tools
MKDIR := mkdir -p
RM    := rm -rf

# Build tools
ifeq ($(strip $(COMPILE)),)
    override COMPILE       := gcc
endif
ifeq ($(strip $(LINK)),)
    override LINK          := ld
endif
ifeq ($(strip $(OBJCOPY)),)
    override OBJCOPY       := objcopy
endif
ifeq ($(strip $(OBJDUMP)),)
    override OBJDUMP       := objdump
endif
ifeq ($(strip $(SIZE)),)
    override SIZE          := size
endif

# Compiler flags
CFLAGS := -std=c17
ifeq ($(COMPILE),arm-none-eabi-gcc)
    CFLAGS += -mcpu=cortex-m0 -Os -nostdlib -ffreestanding
    CFLAGS += -fno-pic -fno-pie -static
    CFLAGS += -ffunction-sections -fdata-sections -fcf-protection=none
    CFLAGS += -fno-jump-tables
endif

# Linker flags
LDFLAGS = $(LINKER_SCRIPT) --gc-sections -static -no-pie --build-id=none

LINKS = \

WARNINGS = \
    -Wall \
    -Wextra \
    -pedantic \
    -Werror \


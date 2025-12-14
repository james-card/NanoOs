# CLI tools
MKDIR := mkdir -p
RM    := rm -rf

# Build tools
ifeq ($(strip $(COMPILE)),)
    override COMPILE       := arm-none-eabi-gcc
endif
ifeq ($(strip $(LINK)),)
    override LINK          := arm-none-eabi-ld
endif
ifeq ($(strip $(OBJCOPY)),)
    override OBJCOPY       := arm-none-eabi-objcopy
endif
ifeq ($(strip $(OBJDUMP)),)
    override OBJDUMP       := arm-none-eabi-objdump
endif
ifeq ($(strip $(SIZE)),)
    override SIZE          := arm-none-eabi-size
endif

ifeq ($(COMPILE),arm-none-eabi-gcc)
    CPU := -mcpu=cortex-m0
endif

# Linker flags
LDFLAGS = $(LINKER_SCRIPT) --gc-sections -static -no-pie --build-id=none

# Compiler flags
CFLAGS = $(CPU) -Os -nostdlib -ffreestanding
CFLAGS += -std=c17 -fno-pic -fno-pie -fno-stack-protector -static
CFLAGS += -ffunction-sections -fdata-sections -fcf-protection=none
CFLAGS += -fno-jump-tables

LINKS = \

WARNINGS = \
    -Wall \
    -Wextra \
    -pedantic \
    -Werror \


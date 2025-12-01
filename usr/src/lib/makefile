#CC            := arm-none-eabi-g++ -std=gnu++17
CC            := arm-none-eabi-gcc -std=gnu17
LD            := arm-none-eabi-ld
OBJCOPY       := arm-none-eabi-objcopy
OBJDUMP       := arm-none-eabi-objdump
SIZE          := arm-none-eabi-size
ARCHIVE       := arm-none-eabi-ar rcs

MAKEFILE      := makefile
EXE_DIR       := ../../exe
INCLUDE_DIR   := ../../include
OBJ_DIR       := ../../obj
EXTRA_FLAGS   := -mcpu=cortex-m0 -Os
EXTRA_DEFINES := \

EXTRA_COMMON_FILES := \

MKDIR        := mkdir -p
COPY         := cp
REMOVE       := rm -f

FLAGS := -g $(EXTRA_FLAGS) $(COMMAND_LINE_FLAGS)

COMMON_FILES := \

SRC_FILES := $(shell ls *.c)

OBJ_FILES :=  $(addprefix $(OBJ_DIR)/,$(patsubst %.c,%.o,$(SRC_FILES)))

INCLUDES := \
    -I../../.. \
    -I$(INCLUDE_DIR) \

LINKS := \

WARNINGS := \
    -Wall \
    -Wextra \
    -Werror \

all: $(OBJ_DIR)/libc.a

$(OBJ_DIR)/libc.a: $(OBJ_FILES)
	@$(MKDIR) $(OBJ_DIR)
	$(ARCHIVE) $@ $(OBJ_FILES)

$(OBJ_DIR)/%.o: %.c $(INCLUDE_DIR)/%.h $(MAKEFILE)
	@$(MKDIR) $(OBJ_DIR)
	$(CC) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -c $< -o $@

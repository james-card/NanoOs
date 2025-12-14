include include.mk

COMMANDS := $(shell ls | grep -Ev 'makefile|.*\.mk')

OBJ_DIR = ../../obj
BIN_DIR = ../../bin

.PHONY: all clean $(COMMANDS)

all: $(COMMANDS)

$(COMMANDS): ./%:
	$(MAKE) -C $@ TARGET="$@" COMPILE=$(COMPILE) LINK=$(LINK) \
		OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) SIZE=$(SIZE)

clean:
	$(RM) $(OBJ_DIR)/start.o
	for command in $(COMMANDS); do \
		$(MAKE) -C $${command} clean TARGET=$${command}; \
	done

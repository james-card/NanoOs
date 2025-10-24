# OBJ_DIR is relative to the command directories
OBJ_DIR       := ../../../obj

COMMANDS := $(shell ls | grep -v makefile)

.PHONY: all clean $(COMMANDS)

all: $(COMMANDS)

$(COMMANDS): ./%:
	$(MAKE) -C $@ COMMAND_LINE_LINKS=

clean:
	for command in $(COMMANDS); do $(MAKE) -C $${command} clean; done

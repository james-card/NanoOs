COMMANDS := $(shell ls | grep -Ev 'makefile|include.mk|overlay.mk')

.PHONY: all clean $(COMMANDS)

all: $(COMMANDS)

$(COMMANDS): ./%:
	$(MAKE) -C $@ TARGET="$@" COMPILE=$(COMPILE) LINK=$(LINK) \
		OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) SIZE=$(SIZE)

clean:
	for command in $(COMMANDS); do \
		$(MAKE) -C $${command} clean TARGET=$${command}; \
	done

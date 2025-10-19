COMMANDS := $(shell ls | grep -v makefile)

.PHONY: all clean $(COMMANDS)

all: $(COMMANDS)

$(COMMANDS): ./%:
	$(MAKE) -C $@

clean:
	for command in $(COMMANDS); do $(MAKE) -C $${command} clean; done

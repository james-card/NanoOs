COMMANDS := $(shell ls | grep -Ev 'makefile|include.mk')

.PHONY: all clean $(COMMANDS)

all: $(COMMANDS)

$(COMMANDS): ./%:
	$(MAKE) -C $@ TARGET="$@"

clean:
	for command in $(COMMANDS); do $(MAKE) -C $${command} clean; done

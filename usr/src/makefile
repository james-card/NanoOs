SOURCES := commands

.PHONY: all clean $(SOURCES)

all: $(SOURCES)

$(SOURCES): ./%:
	$(MAKE) -C $@ COMPILE=$(COMPILE) LINK=$(LINK) OBJCOPY=$(OBJCOPY) \
		OBJDUMP=$(OBJDUMP) SIZE=$(SIZE)

clean:
	for command in $(SOURCES); do $(MAKE) -C $${command} clean; done

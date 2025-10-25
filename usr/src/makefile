SOURCES := commands

.PHONY: all clean $(SOURCES)

all: $(SOURCES)

$(SOURCES): ./%:
	$(MAKE) -C $@

clean:
	for command in $(SOURCES); do $(MAKE) -C $${command} clean; done

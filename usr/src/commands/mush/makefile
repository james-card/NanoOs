include ../include.mk

OVERLAYS := $(shell ls | grep -Ev 'makefile|include.mk')

.PHONY: all clean $(OVERLAYS)

all: $(OVERLAYS)

$(OVERLAYS): ./%:
	$(MAKE) -C $@ OVERLAY=$@ COMPILE=$(COMPILE) LINK=$(LINK) \
		OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) SIZE=$(SIZE) \
		TARGET=$(TARGET)

clean:
	for overlay in $(OVERLAYS); do \
		$(MAKE) -C $${overlay} clean TARGET=$${overlay}; \
	done

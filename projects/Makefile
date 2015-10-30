PROJECTS=libplf plfinfo plf_inst_extract plftool usb_flash
.PHONY:	all

all:
	for project in $(PROJECTS); do \
		$(MAKE) -C "$$project" ; \
	done

SUBDIRS = hal xtos include

bin_PROGRAMS = \
	reef

# generate linker script from platform headers
LINK_SCRIPT = ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)
LINK_DEPS = \
	../../platform/$(PLATFORM)/include/platform/memory.h \
	../../platform/$(PLATFORM)/include/xtensa/config/core-isa*

nodist_reef_SOURCES = $(LINK_SCRIPT).in
BUILT_SOURCES = $(LINK_SCRIPT)
CLEANFILES = $(LINK_SCRIPT)
$(LINK_SCRIPT): Makefile $(LINK_SCRIPT).in $(LINK_DEPS)
	cat $(LINK_SCRIPT).in | $(CPP) -P $(PLATFORM_INCDIR) $(REEF_INCDIR) - >$@

noinst_LIBRARIES = \
	libreset.a

libreset_a_SOURCES = \
	reset-vector.S

libreset_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	-mtext-section-literals

reef_SOURCES = \
	crt1-boards.S \
	_vectors.S \
	init.c \
	wait.S \
	timer.c \
	task.c

reef_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(REEF_INCDIR) \
	$(PLATFORM_INCDIR)

reef_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

reef_LDADD = \
	../../init/libinit.a \
	../../platform/$(PLATFORM)/libplatform.a \
	../../tasks/libtasks.a \
	../../lib/libcore.a \
	../../ipc/libipc.a \
	../../audio/libaudio.a \
	../../drivers/libdrivers.a \
	libreset.a \
	xtos/libxtos.a \
	xtos/libxlevel2.a \
	xtos/libxlevel3.a \
	xtos/libxlevel4.a \
	xtos/libxlevel5.a \
	hal/libhal.a \
	-lgcc

reef_LDFLAGS = \
	$(ARCH_LDFLAGS) \
	-T ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)

bin-local: reef
	$(OBJCOPY) -O binary reef reef-$(FW_NAME).bin
	$(OBJDUMP) -h -D reef > reef-$(FW_NAME).map
	rimage -i reef -o reef-$(FW_NAME).ri -m $(FW_NAME)

vminstall-local:
	scp -P 5555 reef-*.ri root@localhost:/lib/firmware/intel/

clean-local:
	rm -f reef-*.bin

SUBDIRS = hal xtos include

bin_PROGRAMS = \
	reef

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
	timer.c

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
	../../ipc/libipc.a \
	../../tasks/libtasks.a \
	../../audio/libaudio.a \
	../../platform/$(PLATFORM)/libplatform.a \
	../../drivers/libdrivers.a \
	../../lib/libcore.a \
	../../ipc/libipc.a \
	../../platform/$(PLATFORM)/libplatform.a \
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
	$(OBJCOPY) -O binary reef reef.bin
	$(OBJDUMP) -D reef > reef.map
	rimage -i reef -o reef-$(FW_NAME).ri -m $(FW_NAME)

vminstall:
	scp -P 5555 reef.ri root@localhost:/lib/firmware/intel/


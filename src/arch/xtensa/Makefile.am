SUBDIRS = hal xtos include

bin_PROGRAMS = \
	reef

reef_SOURCES = \
	crt1-boards.S \
	init.c

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
	../../lib/libcore.a \
	../../init/libinit.a \
	../../ipc/libipc.a \
	../../tasks/libtasks.a \
	../../audio/libaudio.a \
	../../drivers/libdrivers.a \
	../../platform/$(PLATFORM)/libplatform.a \
	hal/libhal.a \
	xtos/libxtos.a \
	-lgcc

reef_LDFLAGS = \
	$(ARCH_LDFLAGS) \
	-T ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)

bin-local: reef
	$(OBJCOPY) -O binary reef reef.bin
	

SUBDIRS = hal xtos include

bin_PROGRAMS = \
	reef

reef_SOURCES = \
	crt1-boards.S \
	init.c

reef_CFLAGS = \
	$(XTENSA_INCDIR) \
	$(XTENSA_CFLAGS)

reef_CCASFLAGS = \
	$(XTENSA_INCDIR) \
	$(XTENSA_ASFLAGS)

reef_LDADD = \
	$(REEF_LIBS) \
	-lgcc \
	-lhal \
	-lxtos

reef_LDFLAGS = \
	$(XTENSA_LIBDIR) \
	$(XTENSA_LDFLAGS) \
	-T ../../platform/$(PLATFORM_DIR)/$(XTENSA_LDSCRIPT) \
	$(REEF_LIBDIR)

bin-local: reef
	$(OBJCOPY) -O binary reef reef.bin

# xtensa-lx200-elf-objcopy -O binary src/arch/xtensa/reef reef.bin

	

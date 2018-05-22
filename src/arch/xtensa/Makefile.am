SUBDIRS = hal xtos include

noinst_PROGRAMS = \
	sof

# generate linker script from platform headers
LINK_SCRIPT = ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)
LINK_DEPS = \
	../../platform/$(PLATFORM)/include/platform/memory.h \
	../../platform/$(PLATFORM)/include/xtensa/config/core-isa*

nodist_sof_SOURCES = $(LINK_SCRIPT).in
BUILT_SOURCES = $(LINK_SCRIPT)
CLEANFILES = $(LINK_SCRIPT)
$(LINK_SCRIPT): Makefile $(LINK_SCRIPT).in $(LINK_DEPS)
	cat $(LINK_SCRIPT).in | $(CPP) -P $(PLATFORM_INCDIR) $(SOF_INCDIR) - >$@

noinst_LIBRARIES = \
	libreset.a

libreset_a_SOURCES = \
	reset-vector.S

libreset_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	-mtext-section-literals

sof_SOURCES = \
	crt1-boards.S \
	_vectors.S \
	init.c \
	timer.c \
	task.c

if BUILD_CANNONLAKE
sof_SOURCES += \
	main-entry.S
endif

sof_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(SOF_INCDIR) \
	$(PLATFORM_INCDIR)

sof_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

sof_LDADD = \
	../../init/libinit.a \
	../../tasks/libtasks.a \
	../../lib/libcore.a \
	../../platform/$(PLATFORM)/libplatform.a \
	../../ipc/libsof_ipc.a \
	../../audio/libaudio.a \
	../../drivers/libdrivers.a \
	../../math/libsof_math.a \
	xtos/libxtos.a \
	xtos/libxlevel2.a \
	xtos/libxlevel3.a \
	xtos/libxlevel4.a \
	xtos/libxlevel5.a \
	hal/libhal.a \
	-lgcc

sof_LDFLAGS = \
	$(ARCH_LDFLAGS) -Wl,-Map=sof-$(FW_NAME).map \
	-T ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)

RIMAGE_FLAGS = sof-$(FW_NAME)
RIMAGE_BOOT_FLAGS =
BIN_FLAGS = sof

if BUILD_BOOTLOADER

noinst_PROGRAMS += \
	boot_ldr

LINK_BOOT_LDR_SCRIPT = ../../platform/$(PLATFORM)/$(PLATFORM_BOOT_LDR_LDSCRIPT)
BUILT_SOURCES += $(LINK_BOOT_LDR_SCRIPT)
CLEANFILES += $(LINK_BOOT_LDR_SCRIPT)

nodist_boot_ldr_SOURCES = $(LINK_BOOT_LDR_SCRIPT).in
$(LINK_BOOT_LDR_SCRIPT): Makefile $(LINK_BOOT_LDR_SCRIPT).in $(LINK_DEPS)
	cat $(LINK_BOOT_LDR_SCRIPT).in | $(CPP) -P $(PLATFORM_INCDIR) $(SOF_INCDIR) - >$@

boot_ldr_SOURCES = \
	boot_entry.S \
	_vectors.S \
	boot_loader.c

boot_ldr_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(SOF_INCDIR) \
	$(PLATFORM_INCDIR)

boot_ldr_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

boot_ldr_LDADD = \
	hal/libhal.a \
	libreset.a \
	-lgcc

boot_ldr_LDFLAGS = \
	$(ARCH_LDFLAGS) \
	-T ../../platform/$(PLATFORM)/$(PLATFORM_BOOT_LDR_LDSCRIPT)

boot_ldr-local:
	cp boot_ldr boot_ldr-$(FW_NAME)
	$(OBJCOPY) -O binary ../../platform/$(PLATFORM)/boot_module mod-boot-$(FW_NAME).bin
	$(OBJCOPY) --add-section .module=mod-boot-$(FW_NAME).bin \
		--set-section-flags .module=load,readonly boot_ldr-$(FW_NAME)
	$(OBJCOPY) -O binary boot_ldr boot_ldr-$(FW_NAME).bin
	$(OBJDUMP) -h -D boot_ldr > boot_ldr-$(FW_NAME).map
	$(OBJDUMP) -S boot_ldr > boot_ldr-$(FW_NAME).lst
	$(OBJDUMP) -D boot_ldr > boot_ldr-$(FW_NAME).dis

RIMAGE_BOOT_FLAGS += boot_ldr-$(FW_NAME)
BIN_FLAGS +=boot_ldr-local
endif

if BUILD_MODULE
MODULE_COPY=$(OBJCOPY) -O binary ../../platform/$(PLATFORM)/module mod-$(FW_NAME).bin
MODULE_INSERT=$(OBJCOPY) --add-section .module=mod-$(FW_NAME).bin \
		--set-section-flags .module=load,readonly sof-$(FW_NAME)
else
MODULE_COPY=
MODULE_INSERT=
endif

bin-local: $(BIN_FLAGS)
	cp sof sof-$(FW_NAME)
	$(MODULE_COPY)
	$(MODULE_INSERT)
	$(OBJDUMP) -S sof-$(FW_NAME) > sof-$(FW_NAME).lst
	$(OBJDUMP) -D sof-$(FW_NAME) > sof-$(FW_NAME).dis
	rimage -o sof-$(FW_NAME).ri -m $(FW_NAME) $(RIMAGE_BOOT_FLAGS) $(RIMAGE_FLAGS)

vminstall-local:
	scp -P 5555 sof-*.ri root@localhost:/lib/firmware/intel/

clean-local:
	rm -fr mod-*
	rm -fr *.bin
	rm -fr *.lst
	rm -fr *.map
	rm -fr *.dis

if BUILD_XTENSA_SMP
SUBDIRS = smp include
else
SUBDIRS = up include
endif

noinst_PROGRAMS = \
	sof

# generate linker script from platform headers
LINK_SCRIPT = ../../platform/$(PLATFORM)/$(PLATFORM_LDSCRIPT)

LINK_DEPS = ../../platform/$(PLATFORM)/include/platform/memory.h

if XCC
LINK_DEPS += \
	$(ROOT_DIR)/arch/include/xtensa/config/core-isa*
else
LINK_DEPS += \
	../../platform/$(PLATFORM)/include/arch/xtensa/config/core-isa*
endif

nodist_sof_SOURCES = $(LINK_SCRIPT).in
BUILT_SOURCES = $(LINK_SCRIPT)
CLEANFILES = $(LINK_SCRIPT)
$(LINK_SCRIPT): Makefile $(LINK_SCRIPT).in $(LINK_DEPS)
	cat $(LINK_SCRIPT).in | $(CPP) -P $(PLATFORM_INCDIR) $(SOF_INCDIR) - >$@

noinst_LIBRARIES = \
	libreset.a

if BUILD_XTENSA_SMP
libreset_a_SOURCES = \
	smp/xtos/memctl_default.S \
	smp/xtos/reset-vector.S
else
libreset_a_SOURCES = \
	up/xtos/reset-vector.S
endif

libreset_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	-mtext-section-literals

sof_SOURCES = \
	timer.c

if BUILD_XTENSA_SMP
sof_SOURCES += \
	smp/xtos/crt1-boards.S \
	smp/xtos/_vectors.S \
	smp/cpu.c \
	smp/init.c \
	smp/schedule.c \
	smp/task.c \
	smp/work.c
else
sof_SOURCES += \
	up/xtos/crt1-boards.S \
	up/xtos/_vectors.S \
	up/cpu.c \
	up/init.c \
	up/schedule.c \
	up/task.c \
	up/work.c
endif

if BUILD_BOOTLOADER
sof_SOURCES += \
	main-entry.S
endif

sof_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

sof_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

sof_LDADD = \
	../../init/libinit.a \
	../../tasks/libtasks.a \
	../../lib/libcore.a \
	../../platform/$(PLATFORM)/libplatform.la \
	../../ipc/libsof_ipc.a \
	../../lib/libdma.a \
	../../audio/libaudio.a \
	../../drivers/libdrivers.a \
	../../math/libsof_math.a \
	-lgcc

if BUILD_XTENSA_SMP
sof_LDADD += \
	smp/xtos/libxtos.a \
	smp/xtos/libxlevel2.a \
	smp/xtos/libxlevel3.a \
	smp/xtos/libxlevel4.a \
	smp/xtos/libxlevel5.a \
	smp/hal/libhal.a
else
sof_LDADD += \
	up/xtos/libxtos.a \
	up/xtos/libxlevel2.a \
	up/xtos/libxlevel3.a \
	up/xtos/libxlevel4.a \
	up/xtos/libxlevel5.a \
	up/hal/libhal.a
endif

if !BUILD_BOOTLOADER
sof_LDADD += \
	libreset.a
endif

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
	boot_loader.c

if BUILD_XTENSA_SMP
boot_ldr_SOURCES += \
	smp/xtos/_vectors.S
else
boot_ldr_SOURCES += \
	up/xtos/_vectors.S
endif

boot_ldr_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

boot_ldr_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

boot_ldr_LDADD = \
	libreset.a \
	-lgcc

if BUILD_XTENSA_SMP
boot_ldr_LDADD += \
	smp/hal/libhal.a
else
boot_ldr_LDADD += \
	up/hal/libhal.a
endif

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

if USE_MEU
RIMAGE=rimage -o sof-$(FW_NAME).ri -m $(FW_NAME) $(RIMAGE_BOOT_FLAGS) $(RIMAGE_FLAGS) -s $(MEU_OFFSET)
MEU=$(MEU_PATH)/meu -w ./ -s sof-$(FW_NAME) -key $(PRIVATE_KEY) -stp /usr/bin/openssl -f $(MEU_PATH)/generic_meu_conf.xml \
	-mnver 0.0.0.0 -o sof-$(FW_NAME).ri
else
RIMAGE=rimage -o sof-$(FW_NAME).ri -m $(FW_NAME) $(RIMAGE_BOOT_FLAGS) $(RIMAGE_FLAGS)
MEU=
endif

bin-local: $(BIN_FLAGS)
	cp sof sof-$(FW_NAME)
	$(MODULE_COPY)
	$(MODULE_INSERT)
	$(OBJDUMP) -S sof-$(FW_NAME) > sof-$(FW_NAME).lst
	$(OBJDUMP) -D sof-$(FW_NAME) > sof-$(FW_NAME).dis
	$(RIMAGE)
	$(MEU)

vminstall-local:
	scp -P 5555 sof-*.ri root@localhost:/lib/firmware/intel/

clean-local:
	rm -fr mod-*
	rm -fr *.bin
	rm -fr *.lst
	rm -fr *.map
	rm -fr *.dis

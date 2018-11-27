export COMMON_INCDIR = \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

if BUILD_LIB
SUBDIRS = ipc math audio arch include library host
endif

if BUILD_XTENSA
SUBDIRS = include gdb init math audio platform tasks drivers ipc lib arch
endif

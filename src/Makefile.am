export COMMON_INCDIR = \
	$(REEF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)

if BUILD_LIB
SUBDIRS = ipc math audio arch include library
endif

if BUILD_XTENSA
SUBDIRS = include init math audio platform tasks drivers ipc lib arch
endif

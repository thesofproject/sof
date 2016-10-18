SUBDIRS = src

ACLOCAL_AMFLAGS = -I m4

SRC_DIR = $(abs_top_builddir)/src

export REEF_INCDIR = \
	-I $(SRC_DIR)/include \
	-I $(ROOT_DIR)/include

export ARCH_INCDIR = \
	-I $(SRC_DIR)/arch/$(ARCH)/include \
	-I $(SRC_DIR)/arch/$(ARCH)/xtos

export PLATFORM_INCDIR = \
	-I $(SRC_DIR)/platform/$(PLATFORM)/include

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
	echo $(VERSION) > $(distdir)/.version

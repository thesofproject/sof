SUBDIRS = src

SRC_DIR = $(abs_top_builddir)/src

export REEF_INCDIR = \
	-I $(SRC_DIR)/include \
	-I $(TOOL_DIR)/include

export ARCH_INCDIR = \
	-I $(SRC_DIR)/arch/$(ARCH)/include \
	-I $(SRC_DIR)/arch/$(ARCH)/xtos

export PLATFORM_INCDIR = \
	-I $(SRC_DIR)/platform/$(PLATFORM)/include


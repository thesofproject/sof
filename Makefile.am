SUBDIRS = src

SRC_DIR = $(abs_top_builddir)/src

export XTENSA_INCDIR = \
	-I $(SRC_DIR)/include \
	-I $(SRC_DIR)/arch/xtensa/include \
	-I $(SRC_DIR)/arch/xtensa/xtos \
	-I $(SRC_DIR)/platform/$(PLATFORM_DIR)/include

export XTENSA_LIBDIR = \
	-L $(SRC_DIR)/arch/xtensa/hal \
	-L $(SRC_DIR)/arch/xtensa/xtos \
	-L /home/lrg/source/xtensa-lpe/tools/xtensa-lx200-elf/lib


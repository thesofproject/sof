# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define debug_config
  CFLAGS += -DDEBUG -g -O1
  CXXFLAGS += $(CFLAGS)
endef

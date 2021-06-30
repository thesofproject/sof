# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define release_config
  CFLAGS += -DNDEBUG -g -O1
  CXXFLAGS += $(CFLAGS)
endef

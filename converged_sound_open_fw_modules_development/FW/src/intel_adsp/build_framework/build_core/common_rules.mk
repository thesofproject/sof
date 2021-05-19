# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


ifndef _COMMON_RULES_MK_
override _COMMON_RULES_MK_ :=1


.PHONY: $(_PLATFORM)
$(_PLATFORM): build

.PHONY: help
help: usage

.PHONY: usage
usage:
	@$(info $(_USAGE_MESSAGE))

.%/:
	$(call mkdir,$@)

$(call linuxpath,$(CURDIR))/%/:
	$(call mkdir,$*)


ifdef $(TOOLSCHAIN)_toolschain_rules
  $(eval $(value $(TOOLSCHAIN)_toolschain_rules))
endif


endif

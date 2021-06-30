# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


ifndef _ERROR_FUNCTIONS_MK_
override _ERROR_FUNCTIONS_MK_ :=1


# list of error functions :
#--------------------------
# error_if_not_defined(a) : "a" variable shall be defined
# error_if_defined(a) : "a" variable shall not be defined
# error_if_not_unique_value(list) : values of "list" variable shall be unique
# error_if_not_subset(subset,set) : list values of "subset" variable shall be a values subset of "set" variable
# error_if_not_single_value(a) : "a" variable shall be undefined or have a single value (i.e. it is not a values list)


define error_if_not_defined_fct
  ifndef $(varname)
    $(error "$(varname)" variable shall be defined !)
  endif
endef
error_if_not_defined = $(foreach varname,$1,$(eval $(value error_if_not_defined_fct)))
#e.g. some error wil be produced about "b" variable
#a := yo
#b :=
#$(call error_if_not_defined,a b)

define error_if_defined_fct
  ifdef $(varname)
    $(error "$(varname)" variable shall not be defined ! $(varname)=$(value $(varname)))
  endif
endef
error_if_defined = $(foreach varname,$1,$(eval $(value error_if_defined_fct)))
#e.g. some error wil be produced about "b" variable
#a :=
#b := yo
#$(call error_if_defined,a b)

define error_if_not_unique_value_fct
  ifneq (,$(strip $(foreach val,$(value $(value_list)),$(subst 1,,$(words $(filter $(val),$(value $(value_list))))))))
    $(error some values in list variable "$(value_list)" are not unique ! $(value_list) = $(value $(value_list)))
  endif
endef
error_if_not_unique_value = $(foreach value_list,$1,$(eval $(value error_if_not_unique_value_fct)))
#e.g. some error wil be produced about "b" variable
#a = yo bob hic
#b = yo bob yo
#$(call error_if_not_unique_value,a b)


define error_if_not_subset_fct
  ifneq ($(words $(value $(subset))),$(words $(foreach val,$(value $(subset)),$(filter $(val),$(value $(fullset))))))
    $(error some value of "$(subset)" variable has not been found in list "$(fullset)" ! $(subset)=$(value $(subset)) $(fullset)=$(value $(fullset)))
  endif
endef
error_if_not_subset = $(foreach fullset,$2,$(foreach subset,$1,$(eval $(value error_if_not_subset_fct))))
#e.g. some error wil be produced about "b" variable
#a := yo bob
#b := yo mec
#c := yo bob tod
#$(call error_if_not_subset,a b,c)

define error_if_not_single_value_fct
  ifdef $(varname)
    ifneq (1, $(words $(value $(varname))))
      $(error "$(varname)" variable shall have one single value ! $(varname)=$(value $(varname)))
    endif
  endif
endef
error_if_not_single_value = $(foreach varname,$1,$(eval $(value error_if_not_single_value_fct)))
#e.g. some error wil be produced about "b" variable
#a := yo
#b := yo bob
#$(call error_if_not_single_value,a b)

define error_if_file_not_found_fct
  ifeq (,$(wildcard $(path)))
      $(error "$(path)" file is unreachable !)
  endif
endef
error_if_file_not_found = $(foreach path,$1,$(eval $(value error_if_file_not_found_fct)))
#e.g. some error wil be produced for 2nd funtion call
#$(call error_if_file_not_found,$(_BUILD_FRMK_DIR))
#$(call error_if_file_not_found,$(_BUILD_FRMK_DIR)_)

endif

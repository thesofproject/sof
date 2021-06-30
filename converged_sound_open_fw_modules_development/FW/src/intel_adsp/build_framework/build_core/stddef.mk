# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define _SPACE_
 
endef

define _COMMA_
,
endef

define _RETURN_


endef

define _COLON_
:
endef

# return true if it is worth 1 or true and return false otherwise
override is_true = $(if $(filter true 1,$1),true,false)

# Returns $1 if $1 is a substring of $2 otherwise return nothing
# $1 and $2 are expected to contain only one single string
override not_in_string = $(if $(findstring $1,$2),,$1)

# turns space-separated values list into comma-separated values list
override csv = $(subst $(_SPACE_),$(_COMMA_),$1)

# stores $1 in given $2 variable name and returns value
override store = $(eval override $2 := $1)$(value $2)

# echo in console and execute the statement $1.
# note that execution result is assigned to a _result variable.
override print_n_do = $(info $1)$(call store,$1,__result)


# $1 function call to evaluate
# $2 expected result
# return nothing
override test_function = \
  $(if $(filter $(call store,$(subst $(_SPACE_),!,$2),_expected),$(subst $(_SPACE_),!,$(call print_n_do,$1))),\
    $(info   --> PASSED, got "$(__result)"),\
    $(if $(filter !$(_expected)!$(__result)!,!!!),$(info   --> PASSED, got "$(__result)"),$(warning FAILED expecting "$2", got "$(__result)"))\
  )

ifneq (,$(BUILD_FWK_UT))
.PHONY:
default::;


$(info Unit testing not_in_string)
$(info --------------------------)
$(call test_function,$$(call not_in_string,AB,abcde),AB)
$(call test_function,$$(call not_in_string,AB,_ABcde),)
$(call test_function,$$(call not_in_string,AB,AB),)
$(call test_function,$$(call not_in_string,AB,defAB),)
$(info not_in_string UT done)

$(info Unit testing csv)
$(info ----------------)
$(call test_function,$$(call csv,AB aa bbc),AB$(_COMMA_)aa$(_COMMA_)bbc)
$(call test_function,$$(call csv,AB aa  bbc),AB$(_COMMA_)aa$(_COMMA_)$(_COMMA_)bbc)
$(call test_function,$$(call csv,),)
$(call test_function,$$(call csv,aa),aa)
$(call test_function,$$(call csv,aa$$(_COMMA_)bb),aa$(_COMMA_)bb)
$(info not_in_string UT done)

endif

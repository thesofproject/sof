# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


ifndef _ARITHMETIC_MK_
_ARITHMETIC_MK_ :=1

override _0   :=
override _1   := !
override _32  := ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !

# indicates max value of $1 and $2
override max = $(words $(subst !!,!,$(join $(value $1),$(value $2))))
# indicates min value of $1 and $2
override min = $(words $(filter x,$(subst !!,x,$(join $(value $1),$(value $2)))))
# return _1 value if first operand is lesser than or equal to second otherwise _0
override compare_le = $(if $(filter $(call min,$1,$2),$(words $(value $1))),$(_1),$(_0))
override compare_ge = $(if $(filter $(call max,$1,$2),$(words $(value $1))),$(_1),$(_0))
override compare_lt = $(if $(filter $(call min,$1,$2),$(words $(value $1))),$(if $(filter $1,$2),$(_0),$(_1)),$(_0))
override compare_eq = $(if $(filter $(value 1),$(value 2)),$(_1),$(_0))
override _add = $(value $1) $(value $2)
# adds values $1 and $2
override add = $(words $(value $1) $(value $2))
override _mult = $(foreach i,$(value $1),$(value $2))
# multiplies $1 and $2
override mult = $(words $(foreach i,$(value $1),$(value $2)))
override _substract = $(filter-out !!,$(join $(value $1),$(value $2)))
# substract $2 from $1
override substract = $(words $(call _substract,$1,$2))
# $1 is the start iterator value, $2 the count of iterations, $3 the expression to eval
override for = $(eval override _i := $(value $1)) $(foreach i,\
                         $(value $2),\
                         $(eval $3)$(eval override _i := $(call _add,_i,_1))\
               )
# the max supported value of integer
override _MAX_INT := 1024
override _1024 := $(call _mult,_32,_32)
# create numbers from 0 to 1024
$(call for,_0,_$(_MAX_INT),$$(eval _$$(words $$(_i))=$$(_i)))

# usage examples
#$(info 2<=4 = $(call compare_le,_2,_4))
#$(info 4<=4 = $(call compare_le,_4,_4))
#$(info 5<=4 = $(call compare_le,_5,_4))
#$(info 35*10 = $(call mult,_35,_10))
#$(info 32-9 = $(call substract,_32,_9))
#$(info 1020+3 = $(call add,_1020,_3))
#$(info min(35,10) = $(call min,_35,_10))
#$(info max(1020,3) = $(call max,_1020,_3))
#$(call for,_2,_5,$$(info _i:=$$(words $$(_i))))
#$(call for,_0,_3,$$(info _i:=$$(call max,_i,_1)))
#$(error )

endif

#ifndef XTENSA_DEFS_H
#define XTENSA_DEFS_H

#include <xtensa/specreg.h>
#include <xtensa/config/core-isa.h>
#include <xtensa/corebits.h>
#include <config.h>

#define _AREG0		256

#define STACK_SIZE	1024
#define DEBUG_PC	(EPC + XCHAL_DEBUGLEVEL)
#define DEBUG_EXCSAVE	(EXCSAVE + XCHAL_DEBUGLEVEL)
#define DEBUG_PS	(EPS + XCHAL_DEBUGLEVEL)

#endif /* XTENSA_DEFS_H */

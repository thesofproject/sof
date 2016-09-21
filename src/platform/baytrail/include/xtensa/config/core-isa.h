#include <config.h>

#ifdef CONFIG_BAYTRAIL
#include <xtensa/config/core-isa-byt.h>
#elif CONFIG_CHERRYTRAIL
#include <xtensa/config/core-isa-cht.h>
#else
#error "No ISA configuration selected"
#endif

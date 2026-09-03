dnl
dnl Memory capabilities.
dnl
dnl These are ORed together to create a capability mask that's sent to the
dnl SOF firmware when creating buffer or allocating other memory resources.
dnl
dnl ** Must match SOF_MEM_CAPS_ values in ipc.h **

define(`MEM_CAP_RAM', eval(1 << 0))
define(`MEM_CAP_ROM', eval(1 << 1))
define(`MEM_CAP_EXT', eval(1 << 2))
define(`MEM_CAP_LP', eval(1 << 3))
define(`MEM_CAP_HP', eval(1 << 4))
define(`MEM_CAP_DMA', eval(1 << 5))
define(`MEM_CAP_CACHE', eval(1 << 6))

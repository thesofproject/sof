# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and the host library
add_local_sources(sof
	handler-kernel.c
	handler-user-kernel.c
	helper-kernel.c
	logging.c
	notification.c
)

# The IPC4 message routing for the LL scheduler in user space or in kernel
# space, see handler-user-kernel.c
if(CONFIG_SOF_USERSPACE_LL)
	add_local_sources(sof handler-ll-user.c)
else()
	add_local_sources(sof handler-ll-kernel.c)
endif()

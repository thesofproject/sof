# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY_MANAGER)
    add_local_sources(sof lib_manager.c, lib_notifications.c)
endif()

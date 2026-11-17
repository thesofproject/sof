/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Private to the kernel IPC4 message routing in src/kernel/ipc/ipc4:
 * handler-user-kernel.c, the routers common to all builds, calls the
 * ipc4_route_*() functions for the messages that are handled differently
 * with the LL scheduler in user space (handler-ll-user.c, built with
 * CONFIG_SOF_USERSPACE_LL) and in kernel space (handler-ll-kernel.c).
 */

#ifndef __KERNEL_IPC_IPC4_HANDLER_ROUTE_H__
#define __KERNEL_IPC_IPC4_HANDLER_ROUTE_H__

#include <stdbool.h>

struct ipc4_message_request;
struct ipc_msg;

/* Command format errors during fuzzing are reported for virtually all
 * commands, and the resulting flood of logging becomes a severe
 * performance penalty (i.e. we get a lot less fuzzing done per CPU
 * cycle).
 */
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
#define ipc_cmd_err(...)
#else
#define ipc_cmd_err(...) tr_err(__VA_ARGS__)
#endif

/* Handlers in handler-user-kernel.c, used by both routing variants */
int ipc4_init_module_instance(struct ipc4_message_request *ipc4);
int ipc4_get_large_config_module_instance(struct ipc4_message_request *ipc4);
int ipc4_set_large_config_module_instance(struct ipc4_message_request *ipc4);

/* Global messages */
int ipc4_route_chain_dma(struct ipc4_message_request *ipc4);
int ipc4_route_create_pipeline(struct ipc4_message_request *ipc4);
int ipc4_route_delete_pipeline(struct ipc4_message_request *ipc4);
int ipc4_route_set_pipeline_state(struct ipc4_message_request *ipc4);

/* Module messages */
int ipc4_route_init_module(struct ipc4_message_request *ipc4);
int ipc4_route_module_config(struct ipc4_message_request *ipc4, bool set,
			     struct ipc_msg *reply);
int ipc4_route_large_config_get(struct ipc4_message_request *ipc4, struct ipc_msg *reply);
int ipc4_route_large_config_set(struct ipc4_message_request *ipc4);
int ipc4_route_bind(struct ipc4_message_request *ipc4);
int ipc4_route_unbind(struct ipc4_message_request *ipc4);
int ipc4_route_delete_module(struct ipc4_message_request *ipc4);

#endif /* __KERNEL_IPC_IPC4_HANDLER_ROUTE_H__ */

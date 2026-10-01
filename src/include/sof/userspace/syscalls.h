/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * \file
 * \brief Map of the SOF user/kernel system-call boundary.
 *
 * This header is a documentation-only index of every SOF system call — the
 * complete user/kernel API surface. It does not declare or include anything;
 * it exists so that the boundary between unprivileged (user) audio code and
 * privileged (kernel) OS/HAL code can be read in one place.
 *
 * For the architecture, directory taxonomy, memory-partition markers and
 * Kconfig hierarchy that this boundary sits in, see the companion
 * \c README.md in this directory.
 *
 * ## What a SOF syscall looks like
 *
 * Each entry below is declared \c __syscall in the header named in its group
 * and implemented as a pair:
 *
 *   - \c z_vrfy_<name>() — kernel-side validator. Runs in supervisor mode,
 *     validates every user-supplied argument (K_SYSCALL_MEMORY_READ/WRITE,
 *     K_SYSCALL_OBJ, k_usermode_from/to_copy), then calls the implementation.
 *     This is the trust boundary.
 *   - \c z_impl_<name>() — the actual logic; callable directly from kernel
 *     code (fast path) or from the validator when the call comes from user
 *     space.
 *
 * The validators for the lib-level calls are collected under
 * \c zephyr/syscall/ (alloc.c, cpu.c, vregion.c, dai.c, sof_dma.c); the
 * remaining validators still live next to their kernel logic and are the
 * migration targets called out in the README.
 *
 * NOTE: keep this list in sync with the actual declarations. It can be
 * regenerated / checked with:
 * \code
 *   grep -rn '__syscall' src/include zephyr/include
 * \endcode
 *
 * ------------------------------------------------------------------------
 *
 * ## IPC — inter-processor communication
 *   Declared: sof/ipc/msg.h, sof/ipc/ipc_reply.h, ipc4/handler.h,
 *             ipc4/notification.h
 *   Kernel side: src/ipc/ipc-common.c, src/ipc/ipc3/helper.c,
 *                src/ipc/ipc4/handler-kernel.c, src/ipc/ipc4/notification.c
 *
 *     void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority);
 *     void ipc_msg_list_remove(struct ipc_msg *msg);
 *     void ipc_msg_reply(struct sof_ipc_reply *reply);
 *     void ipc_compound_pre_start(int msg_id);
 *     void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed);
 *     int  ipc_wait_for_compound_msg(void);
 *     bool send_resource_notif(uint32_t resource_id, uint32_t event_type,
 *                              uint32_t data);
 *
 * ## Heap allocation
 *   Declared: rtos/alloc.h
 *   Kernel side: zephyr/syscall/alloc.c (z_vrfy_) / zephyr/lib/alloc.c (z_impl_)
 *
 *     void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
 *                          size_t align);
 *     void  sof_heap_free(struct k_heap *heap, void *addr);
 *
 * ## Virtual regions (vregion allocator)
 *   Declared: sof/lib/vregion.h
 *   Kernel side: zephyr/syscall/vregion.c (z_vrfy_) / zephyr/lib/vregion.c
 *
 *     struct vregion *vregion_create_map(uintptr_t *vreg_start, size_t *vreg_size);
 *     void  vregion_set_interim(struct vregion *vr);
 *     struct vregion *vregion_get(struct vregion *vr);
 *     struct vregion *vregion_put(struct vregion *vr);
 *     void *vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment);
 *     void *vregion_alloc_coherent_align(struct vregion *vr, size_t size,
 *                                        size_t alignment);
 *     void  vregion_free(struct vregion *vr, void *ptr);
 *
 * ## DMA
 *   Declared: sof/lib/sof_dma.h
 *   Kernel side: zephyr/syscall/sof_dma.c (z_vrfy_) / src/lib/dma.c (z_impl_)
 *
 *     struct sof_dma *sof_dma_get(uint32_t dir, uint32_t caps, uint32_t dev,
 *                                 uint32_t flags);
 *     void sof_dma_put(struct sof_dma *dma);
 *     int  sof_dma_get_attribute(struct sof_dma *dma, uint32_t type, uint32_t *value);
 *     int  sof_dma_request_channel(struct sof_dma *dma, uint32_t stream_tag);
 *     void sof_dma_release_channel(struct sof_dma *dma, uint32_t channel);
 *     int  sof_dma_config(struct sof_dma *dma, uint32_t channel, ...);
 *     int  sof_dma_start(struct sof_dma *dma, uint32_t channel);
 *     int  sof_dma_stop(struct sof_dma *dma, uint32_t channel);
 *     int  sof_dma_get_status(struct sof_dma *dma, uint32_t channel,
 *                             struct dma_status *stat);
 *     int  sof_dma_reload(struct sof_dma *dma, uint32_t channel, size_t size);
 *     int  sof_dma_suspend(struct sof_dma *dma, uint32_t channel);
 *     int  sof_dma_resume(struct sof_dma *dma, uint32_t channel);
 *
 * ## DAI
 *   Declared: sof/lib/dai-zephyr.h
 *   Kernel side: zephyr/syscall/dai.c (z_vrfy_) / src/lib/dai.c (z_impl_)
 *
 *     const struct device *dai_get_device(enum sof_ipc_dai_type type, uint32_t index);
 *
 * ## CPU
 *   Declared: sof/lib/cpu.h
 *   Kernel side: zephyr/syscall/cpu.c (z_vrfy_) / zephyr/lib/cpu.c (z_impl_)
 *
 *     int cpu_get_id(void);
 *
 * ## Module management (module adapter)
 *   Declared: sof/audio/module_adapter/module/generic.h
 *   Kernel side: src/audio/module_adapter/module/generic.c
 *
 *     void *mod_balloc_align(struct processing_module *mod, size_t size,
 *                            size_t alignment);
 *     void *mod_alloc_ext(struct processing_module *mod, uint32_t flags,
 *                         size_t size, size_t align);
 *     int   mod_free(struct processing_module *mod, const void *ptr);
 *     void  mod_free_all(struct processing_module *mod);
 *     struct comp_data_blob_handler *mod_data_blob_handler_new(struct processing_module *mod);
 *     const void *mod_fast_get(struct processing_module *mod,
 *                              const void * const dram_ptr, size_t size);
 *
 * ## Library manager
 *   Declared: sof/lib_manager.h
 *   Kernel side: src/library_manager/lib_manager.c
 *
 *     int lib_manager_free_module(const uint32_t component_id);
 *
 * ## Scheduling (LL and DP)
 *   Declared: sof/schedule/ll_schedule_domain.h, sof/schedule/dp_schedule.h
 *   Kernel side: src/schedule/zephyr_ll.c,
 *                src/schedule/zephyr_dp_schedule.c,
 *                src/schedule/zephyr_dp_schedule_application.c
 *
 *     int  zephyr_ll_task_sem_alloc(struct task *task);
 *     int  zephyr_ll_task_sem_free(struct task *task);
 *     void scheduler_dp_internal_free(struct task *task);
 *     void scheduler_dp_ll_tick(void);
 *
 * ## Debug
 *   Declared: user/debug_stream_slot.h
 *   Kernel side: src/debug/debug_stream/debug_stream_slot.c
 *
 *     int debug_stream_slot_send_record(struct debug_stream_record *rec);
 */

#ifndef __SOF_USERSPACE_SYSCALLS_H__
#define __SOF_USERSPACE_SYSCALLS_H__

/*
 * Intentionally empty. This file documents the syscall boundary; the actual
 * declarations live in the per-subsystem headers listed above so that each
 * translation unit pulls in only what it needs.
 */

#endif /* __SOF_USERSPACE_SYSCALLS_H__ */

// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Tests for the LL user heap, whose allocator runs in user mode: LL user
 * threads allocate from it without system calls, kernel code refuses to
 * use it, and a user thread that corrupts its metadata can only fault
 * itself. Also checks that the module resource system calls reject LL
 * modules and forged allocation contexts.
 *
 * Tests for the fast_get area of the LL memory region: LL user threads
 * get shared read-only copies with fast_get_ll(), freed copies are reused
 * without stale data, only the LL and IPC user threads may make the calls,
 * and kernel code does not release LL copies with fast_put(). LL modules
 * fall back to a private copy when the area is full.
 */

#include <sof/boot_test.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/lib/fast-get.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/userspace/syscall_caller.h>
#include <rtos/alloc.h>

#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/internal/syscall_handler.h>
#include <zephyr/sys/kobject.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/sys/sys_heap.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048
/* words of the heap header that the corruption case overwrites */
#define HDR_WORDS	32

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

K_APPMEM_PARTITION_DEFINE(ll_user_heap_test_part);

/* test objects that user threads write, so in a user partition */
K_APP_BMEM(ll_user_heap_test_part) static struct processing_module fake_mod;
K_APP_BMEM(ll_user_heap_test_part) static struct comp_dev fake_dev;
K_APP_BMEM(ll_user_heap_test_part) static struct mod_alloc_ctx fake_alloc;
K_APP_BMEM(ll_user_heap_test_part) static void *user_ptr;
K_APP_BMEM(ll_user_heap_test_part) static uint32_t *user_hdr;
K_APP_BMEM(ll_user_heap_test_part) static uint32_t user_hdr_value;

/* kernel-only: a forged heap or vregion handle points here */
static uint32_t kernel_only_words[16];

static uint32_t saved_hdr[HDR_WORDS];

enum user_heap_test_case {
	/* allocate, check and free memory */
	UHEAP_ALLOC_FREE,
	/* allocate and keep the memory in user_ptr */
	UHEAP_ALLOC_KEEP,
	/* free user_ptr */
	UHEAP_FREE_KEPT,
	/* overwrite the heap header with user_hdr_value */
	UHEAP_CORRUPT,
	/* allocate from the corrupted heap */
	UHEAP_ALLOC_CORRUPTED,
	/* allocate from a heap that is not the LL user heap */
	UHEAP_ALLOC_OTHER_HEAP,
	/* call the DP resource system call with fake_mod */
	UHEAP_MOD_DP_SYSCALL,
	/* call the module allocator in user mode with fake_mod */
	UHEAP_MOD_USER_ALLOC,
};

static void check_alloc(uint32_t flags, size_t bytes, size_t align)
{
	struct zephyr_ll_user_heap_stats before, after;
	struct k_heap *heap = zephyr_ll_user_heap();
	uint8_t *ptr;

	zephyr_ll_user_heap_stats_get(&before);

	ptr = sof_heap_alloc(heap, flags, bytes, align);
	zassert_not_null(ptr);
	zassert_equal((uintptr_t)ptr % MAX(align, PLATFORM_DCACHE_ALIGN), 0, "%p misaligned",
		      ptr);
	/* only mirrored cached and uncached aliases can be told apart */
	if (IS_ENABLED(CONFIG_CACHE_MANAGEMENT) &&
	    IS_ENABLED(CONFIG_CACHE_HAS_MIRRORED_MEMORY_REGIONS))
		zassert_equal(sys_cache_is_ptr_cached(ptr), !(flags & SOF_MEM_FLAG_COHERENT));
	memset(ptr, 0x5a, bytes);

	zephyr_ll_user_heap_stats_get(&after);
	zassert_true(after.allocated_bytes >= before.allocated_bytes + bytes);

	sof_heap_free(heap, ptr);

	zephyr_ll_user_heap_stats_get(&after);
	zassert_equal(after.allocated_bytes, before.allocated_bytes);
}

static void user_function(void *p1, void *p2, void *p3)
{
	enum user_heap_test_case test_case = (enum user_heap_test_case)(uintptr_t)p1;
	struct k_heap *heap = zephyr_ll_user_heap();
	int i;

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case UHEAP_ALLOC_FREE:
		check_alloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, 100, 0);
		check_alloc(SOF_MEM_FLAG_USER, 100, 256);
		check_alloc(SOF_MEM_FLAG_USER, 3000, 4096);
		return;
	case UHEAP_ALLOC_KEEP:
		user_ptr = sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 64, 0);
		zassert_not_null(user_ptr);
		return;
	case UHEAP_FREE_KEPT:
		sof_heap_free(heap, user_ptr);
		user_ptr = NULL;
		return;
	case UHEAP_CORRUPT:
		/* the heap metadata is user-writable by design */
		for (i = 0; i < HDR_WORDS; i++)
			user_hdr[i] = user_hdr_value;
		return;
	case UHEAP_ALLOC_CORRUPTED:
		/* may fault, and may also return garbage */
		(void)sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 64, 0);
		return;
	case UHEAP_ALLOC_OTHER_HEAP:
		(void)sof_heap_alloc((struct k_heap *)kernel_only_words, SOF_MEM_FLAG_USER, 64, 0);
		break;
	case UHEAP_MOD_DP_SYSCALL:
		(void)mod_dp_alloc_ext(&fake_mod, SOF_MEM_FLAG_USER, 64, 0);
		break;
	case UHEAP_MOD_USER_ALLOC:
		(void)mod_alloc_ext(&fake_mod, SOF_MEM_FLAG_USER, 64, 0);
		break;
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("user thread did not fault");
}

/* @infra: run as the IPC user thread, otherwise as another LL domain thread */
static void run_user(k_thread_entry_t entry, int test_case, bool infra, bool fault_allowed)
{
	int ret;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			entry, (void *)(uintptr_t)test_case, NULL, NULL,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);
	if (infra)
		sof_sysuser_thread_set(SOF_SYSUSER_IPC, PLATFORM_PRIMARY_CORE_ID, &user_thread);

	if (fault_allowed)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);

	sof_boot_test_set_fault_valid(NULL);
	if (infra)
		sof_sysuser_thread_set(SOF_SYSUSER_IPC, PLATFORM_PRIMARY_CORE_ID, NULL);

	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

static void run_case(enum user_heap_test_case test_case, bool fault_allowed)
{
	run_user(user_function, test_case, false, fault_allowed);
}

static void *ll_user_heap_setup(void)
{
	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &ll_user_heap_test_part);

	return NULL;
}

static void ll_user_heap_teardown(void *fixture)
{
	k_mem_domain_remove_partition(zephyr_ll_mem_domain(), &ll_user_heap_test_part);
}

ZTEST(userspace_ll_user_heap, user_alloc_free)
{
	run_case(UHEAP_ALLOC_FREE, false);
}

ZTEST(userspace_ll_user_heap, kernel_use_refused)
{
	struct zephyr_ll_user_heap_stats before, after;
	struct k_heap *heap = zephyr_ll_user_heap();

	run_case(UHEAP_ALLOC_KEEP, false);
	zephyr_ll_user_heap_stats_get(&before);

	/* kernel code does not run the allocator: nothing is allocated or freed */
	zassert_is_null(sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 64, 0));
	zassert_is_null(sof_heap_alloc(heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, 64, 0));
	sof_heap_free(heap, user_ptr);

	zephyr_ll_user_heap_stats_get(&after);
	zassert_equal(after.allocated_bytes, before.allocated_bytes);

	run_case(UHEAP_FREE_KEPT, false);
}

ZTEST(userspace_ll_user_heap, user_rejects_other_heaps)
{
	run_case(UHEAP_ALLOC_OTHER_HEAP, true);
}

ZTEST(userspace_ll_user_heap, corrupted_metadata_faults_user_only)
{
	struct zephyr_ll_user_heap_stats stats;
	struct k_heap *heap = zephyr_ll_user_heap();
	struct sys_mutex *lock;
	struct sys_heap *sh;
#ifdef CONFIG_SYS_MUTEX_IMPL_K_MUTEX
	struct k_object *ko;
#endif

	zephyr_ll_user_heap_test_get(&sh, &lock);
	user_hdr = (uint32_t *)sh->heap;
	memcpy(saved_hdr, user_hdr, sizeof(saved_hdr));

	/*
	 * Make every chunk index in the header point at address 0x100, so
	 * that following one faults instead of writing to LL memory.
	 */
	user_hdr_value = (uint32_t)((0x100 - (uintptr_t)user_hdr) / 8);

	run_case(UHEAP_CORRUPT, false);
	run_case(UHEAP_ALLOC_CORRUPTED, true);

	/* the paths that used to run the allocator in the kernel */
	zassert_is_null(sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 64, 0));
	sof_heap_free(heap, (uint8_t *)user_hdr + 4096);
	zephyr_ll_user_heap_stats_get(&stats);

	/* the faulting thread may have died holding the heap lock */
#ifdef CONFIG_SYS_MUTEX_IMPL_K_MUTEX
	ko = k_object_find(lock);
	zassert_not_null(ko);
	k_mutex_init(ko->data.mutex);
#else
	sys_mutex_init(lock);
#endif

	memcpy(user_hdr, saved_hdr, sizeof(saved_hdr));

	run_case(UHEAP_ALLOC_FREE, false);
}

ZTEST(userspace_ll_user_heap, mod_dp_rejects_ll_module)
{
	memset(&fake_mod, 0, sizeof(fake_mod));
	memset(&fake_dev, 0, sizeof(fake_dev));
	fake_mod.dev = &fake_dev;
	fake_dev.mod = &fake_mod;
	fake_mod.priv.resources.alloc = &fake_alloc;
	fake_alloc.heap = zephyr_ll_user_heap();
	fake_alloc.vreg = NULL;

	fake_dev.ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
	run_case(UHEAP_MOD_DP_SYSCALL, true);
}

ZTEST(userspace_ll_user_heap, mod_dp_rejects_forged_context)
{
	memset(&fake_mod, 0, sizeof(fake_mod));
	memset(&fake_dev, 0, sizeof(fake_dev));
	fake_mod.dev = &fake_dev;
	fake_dev.mod = &fake_mod;
	fake_mod.priv.resources.alloc = &fake_alloc;
	fake_dev.ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_DP;

	/* a made-up heap, no vregion */
	fake_alloc.heap = (struct k_heap *)kernel_only_words;
	fake_alloc.vreg = NULL;
	run_case(UHEAP_MOD_DP_SYSCALL, true);

	/* a made-up vregion */
	fake_alloc.heap = NULL;
	fake_alloc.vreg = (struct vregion *)kernel_only_words;
	run_case(UHEAP_MOD_DP_SYSCALL, true);
}

ZTEST(userspace_ll_user_heap, mod_user_rejects_forged_heap)
{
	memset(&fake_mod, 0, sizeof(fake_mod));
	memset(&fake_dev, 0, sizeof(fake_dev));
	fake_mod.dev = &fake_dev;
	fake_dev.mod = &fake_mod;
	fake_mod.priv.resources.alloc = &fake_alloc;
	fake_dev.ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
	fake_alloc.heap = (struct k_heap *)kernel_only_words;
	fake_alloc.vreg = NULL;
	mod_resource_init(&fake_mod);

	/* the allocator runs in user mode and refuses the heap */
	run_case(UHEAP_MOD_USER_ALLOC, true);
}

ZTEST_SUITE(userspace_ll_user_heap, sof_boot_test_standalone_predicate, ll_user_heap_setup,
	    NULL, NULL, ll_user_heap_teardown);

/* without DRAM execution fast_get_ll() is a passthrough and there are no copies */
#define HAVE_LL_FAST_GET_COPIES (CONFIG_FAST_GET && CONFIG_COLD_STORE_EXECUTE_DRAM)

#if HAVE_LL_FAST_GET_COPIES
#define DATA_SIZE	1000
/* more than the fast_get area can hold */
#define FG_MAX_BLOCKS	(CONFIG_SOF_ZEPHYR_LL_FG_AREA_SIZE / 256 + 1)

/* source data and results, the user threads read and write them */
K_APP_BMEM(ll_user_heap_test_part) static uint8_t data_a[DATA_SIZE];
K_APP_BMEM(ll_user_heap_test_part) static uint8_t data_b[DATA_SIZE];
K_APP_BMEM(ll_user_heap_test_part) static const void *fg_ptr[2];

/* kernel-only, a user thread must not get a copy of it */
static uint8_t kernel_only_data[DATA_SIZE];

static void *fg_blocks[FG_MAX_BLOCKS];

enum fast_get_test_case {
	/* get data_a twice, check the copies, keep them in fg_ptr */
	FG_GET_SHARED,
	/* put the copies in fg_ptr */
	FG_PUT_KEPT,
	/* get data_a, check it and put it, then the same for data_b */
	FG_GET_PUT_A_THEN_B,
	/* write to a copy */
	FG_WRITE_COPY,
	/* get a copy of kernel memory */
	FG_GET_KERNEL_DATA,
	/* get data_a with the module interface, keep it in fg_ptr[0] */
	FG_MOD_GET,
	/* put fg_ptr[0] with the module interface */
	FG_MOD_PUT,
	/* free the module resources */
	FG_MOD_FREE_ALL,
};

static void check_copy(const void *copy, const uint8_t *data)
{
	zassert_not_null(copy);
	zassert_not_equal(copy, data);
	zassert_true(sys_cache_is_ptr_cached((void *)copy));
	zassert_mem_equal(copy, data, DATA_SIZE);
}

static void fast_get_user_function(void *p1, void *p2, void *p3)
{
	enum fast_get_test_case test_case = (enum fast_get_test_case)(uintptr_t)p1;
	const void *ptr;

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case FG_GET_SHARED:
		fg_ptr[0] = fast_get_ll(data_a, DATA_SIZE);
		check_copy(fg_ptr[0], data_a);
		fg_ptr[1] = fast_get_ll(data_a, DATA_SIZE);
		zassert_equal(fg_ptr[1], fg_ptr[0], "copy not shared");
		/* a size that does not match the shared copy */
		zassert_is_null(fast_get_ll(data_a, DATA_SIZE / 2));
		return;
	case FG_PUT_KEPT:
		fast_put_ll(fg_ptr[0]);
		fast_put_ll(fg_ptr[1]);
		return;
	case FG_GET_PUT_A_THEN_B:
		/* reads data_a into the cache, a later copy may reuse the lines */
		fg_ptr[0] = fast_get_ll(data_a, DATA_SIZE);
		check_copy(fg_ptr[0], data_a);
		fast_put_ll(fg_ptr[0]);
		fg_ptr[1] = fast_get_ll(data_b, DATA_SIZE);
		check_copy(fg_ptr[1], data_b);
		fast_put_ll(fg_ptr[1]);
		return;
	case FG_WRITE_COPY:
		ptr = fast_get_ll(data_a, DATA_SIZE);
		zassert_not_null(ptr);
		fg_ptr[0] = ptr;
		*(volatile uint8_t *)ptr = 0;
		break;
	case FG_GET_KERNEL_DATA:
		(void)fast_get_ll(kernel_only_data, DATA_SIZE);
		break;
	case FG_MOD_GET:
		fg_ptr[0] = mod_fast_get(&fake_mod, data_a, DATA_SIZE);
		check_copy(fg_ptr[0], data_a);
		return;
	case FG_MOD_PUT:
		mod_fast_put(&fake_mod, fg_ptr[0]);
		return;
	case FG_MOD_FREE_ALL:
		mod_free_all(&fake_mod);
		return;
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("user thread did not fault");
}

static void run_fg_case(enum fast_get_test_case test_case, bool infra, bool fault_allowed)
{
	run_user(fast_get_user_function, test_case, infra, fault_allowed);
}

static void *ll_fast_get_setup(void)
{
	int i;

	for (i = 0; i < DATA_SIZE; i++) {
		data_a[i] = i;
		data_b[i] = ~i;
	}

	return ll_user_heap_setup();
}

ZTEST(userspace_ll_fast_get, shared_copy)
{
	size_t used = zephyr_ll_fg_used();

	run_fg_case(FG_GET_SHARED, true, false);
	zassert_true(zephyr_ll_fg_contains(fg_ptr[0], DATA_SIZE));
	zassert_equal(zephyr_ll_fg_used(), used + ROUND_UP(DATA_SIZE, 256));

	/* kernel code does not release LL copies with fast_put() */
	fast_put(NULL, NULL, fg_ptr[0]);
	zassert_equal(zephyr_ll_fg_used(), used + ROUND_UP(DATA_SIZE, 256));

	/* the copy is freed with its last user */
	run_fg_case(FG_PUT_KEPT, true, false);
	zassert_equal(zephyr_ll_fg_used(), used);
}

ZTEST(userspace_ll_fast_get, reuse_without_stale_data)
{
	size_t used = zephyr_ll_fg_used();

	run_fg_case(FG_GET_PUT_A_THEN_B, true, false);
	zassert_equal(fg_ptr[1], fg_ptr[0], "freed copy not reused");
	zassert_equal(zephyr_ll_fg_used(), used);
}

ZTEST(userspace_ll_fast_get, copy_read_only)
{
	size_t used = zephyr_ll_fg_used();

	run_fg_case(FG_WRITE_COPY, true, true);
	zassert_mem_equal(fg_ptr[0], data_a, DATA_SIZE);

	/* the faulting thread kept its reference */
	fast_put_ll(fg_ptr[0]);
	zassert_equal(zephyr_ll_fg_used(), used);
}

ZTEST(userspace_ll_fast_get, caller_checks)
{
	size_t used = zephyr_ll_fg_used();

	/* an LL domain thread that is not the LL or IPC user thread */
	if (IS_ENABLED(CONFIG_SOF_USERSPACE_SYSCALL_CALLER_CHECK))
		run_fg_case(FG_GET_SHARED, false, true);
	/* memory that the caller cannot read */
	run_fg_case(FG_GET_KERNEL_DATA, true, true);

	zassert_equal(zephyr_ll_fg_used(), used);
}

ZTEST(userspace_ll_fast_get, mod_fast_get_falls_back)
{
	size_t used = zephyr_ll_fg_used();
	int n;

	memset(&fake_mod, 0, sizeof(fake_mod));
	memset(&fake_dev, 0, sizeof(fake_dev));
	fake_mod.dev = &fake_dev;
	fake_dev.mod = &fake_mod;
	fake_mod.priv.resources.alloc = &fake_alloc;
	fake_dev.ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
	fake_alloc.heap = zephyr_ll_user_heap();
	fake_alloc.vreg = NULL;
	mod_resource_init(&fake_mod);

	run_fg_case(FG_MOD_GET, true, false);
	zassert_true(zephyr_ll_fg_contains(fg_ptr[0], DATA_SIZE));
	run_fg_case(FG_MOD_PUT, true, false);
	zassert_equal(zephyr_ll_fg_used(), used);

	/* fill the area */
	for (n = 0; n < FG_MAX_BLOCKS; n++) {
		fg_blocks[n] = zephyr_ll_fg_alloc(256);
		if (!fg_blocks[n])
			break;
	}
	zassert_true(n < FG_MAX_BLOCKS, "fast_get area not full");

	run_fg_case(FG_MOD_GET, true, false);
	zassert_false(zephyr_ll_fg_contains(fg_ptr[0], 1), "copy in the full area");
	run_fg_case(FG_MOD_PUT, true, false);

	while (n--)
		zephyr_ll_fg_free(fg_blocks[n], 256);
	zassert_equal(zephyr_ll_fg_used(), used);

	run_fg_case(FG_MOD_FREE_ALL, true, false);
}

static bool ll_fast_get_predicate(const void *state)
{
	return CONFIG_SOF_ZEPHYR_LL_FG_AREA_SIZE && sof_boot_test_standalone_predicate(state);
}

ZTEST_SUITE(userspace_ll_fast_get, ll_fast_get_predicate, ll_fast_get_setup,
	    NULL, NULL, ll_user_heap_teardown);

#endif /* HAVE_LL_FAST_GET_COPIES */


/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_ll_user_heap, false, 1, 1, NULL);
#if HAVE_LL_FAST_GET_COPIES && CONFIG_SOF_ZEPHYR_LL_FG_AREA_SIZE
	ztest_run_test_suite(userspace_ll_fast_get, false, 1, 1, NULL);
#endif
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);

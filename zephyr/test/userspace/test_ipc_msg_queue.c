// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Tests for IPC messages queued by user threads: the kernel queues its own
 * copy of the message, so later changes to the user message, forged list
 * pointers or callbacks have no effect on the kernel send queue.
 *
 * IPC is not started in standalone boot tests, so queued messages are not
 * sent. The tests inspect the kernel send queue directly and drop the test
 * messages with ipc_user_msg_purge().
 */

#include <sof/boot_test.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule_domain.h>

#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

#include <string.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

/* not a valid IPC4 header, never sent: IPC is not started in these tests */
#define TEST_HEADER	0x5ec0de00
#define TEST_MSGS	(CONFIG_SOF_IPC_USER_MSG_SLOTS + 2)

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

/*
 * Messages are sent from two memory domains: a domain of the test, and the
 * LL domain, to which the test partition is added. MMU page tables are
 * scarce, so the second domain is not a new one.
 */
static struct k_mem_domain domain_a;
static struct k_mem_domain *domain_b;

/* kernel-only data, used as forged pointers */
static uint32_t kernel_only_word[16];

static void planted_callback(struct ipc_msg *msg)
{
	kernel_only_word[0] = 0xdead;
}

/* user data, accessible to both test domains */
K_APPMEM_PARTITION_DEFINE(ipc_msg_test_part);
K_APP_BMEM(ipc_msg_test_part) static struct ipc_msg test_msgs[TEST_MSGS];
/* large enough for MSG_SEND_OVERSIZE, so that only the size check can fault */
K_APP_BMEM(ipc_msg_test_part) static uint32_t
	test_payload[DIV_ROUND_UP(CONFIG_SOF_IPC_USER_MSG_MAX_SIZE + 1, sizeof(uint32_t))];
K_APP_BMEM(ipc_msg_test_part) static uint32_t test_data[4];

enum msg_test_case {
	/* send, then change the message and forge its list and callback */
	MSG_SEND_THEN_CHANGE,
	/* send a message with forged list and callback fields */
	MSG_SEND_FORGED_FIELDS,
	/* send with a separate data buffer */
	MSG_SEND_DATA,
	/* send the same message twice */
	MSG_SEND_TWICE,
	/* send a payload larger than CONFIG_SOF_IPC_USER_MSG_MAX_SIZE */
	MSG_SEND_OVERSIZE,
	/* send with a kernel address as the payload */
	MSG_SEND_KERNEL_PAYLOAD,
	/* send more messages than there are slots */
	MSG_SEND_MANY,
	/* remove test_msgs[0] */
	MSG_REMOVE,
};

static void msg_set(struct ipc_msg *msg, uint32_t header, uint32_t size)
{
	memset(msg, 0, sizeof(*msg));
	msg->header = header;
	msg->tx_size = size;
	msg->tx_data = test_payload;
	list_init(&msg->list);
}

static void user_function(void *p1, void *p2, void *p3)
{
	enum msg_test_case test_case = (enum msg_test_case)(uintptr_t)p1;
	struct ipc_msg *msg = &test_msgs[0];
	int i;

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case MSG_SEND_THEN_CHANGE:
		msg_set(msg, TEST_HEADER, 8);
		test_payload[0] = 1;
		test_payload[1] = 2;
		ipc_msg_send(msg, NULL, false);

		/* none of this may affect the queued message */
		test_payload[0] = 9;
		test_payload[1] = 9;
		msg->header = TEST_HEADER + 1;
		msg->tx_size = 4096;
		msg->tx_data = kernel_only_word;
		msg->list.next = (struct list_item *)kernel_only_word;
		msg->list.prev = (struct list_item *)kernel_only_word;
		msg->callback = planted_callback;
		return;
	case MSG_SEND_FORGED_FIELDS:
		msg_set(msg, TEST_HEADER, 4);
		test_payload[0] = 3;
		msg->list.next = (struct list_item *)kernel_only_word;
		msg->list.prev = (struct list_item *)kernel_only_word;
		msg->callback = planted_callback;
		ipc_msg_send(msg, NULL, false);
		return;
	case MSG_SEND_DATA:
		msg_set(msg, TEST_HEADER, 4);
		test_payload[0] = 0;
		test_data[0] = 4;
		ipc_msg_send(msg, test_data, false);
		/* the message payload is updated from data, as for kernel messages */
		zassert_equal(test_payload[0], 4);
		return;
	case MSG_SEND_TWICE:
		msg_set(msg, TEST_HEADER, 4);
		test_payload[0] = 5;
		ipc_msg_send(msg, NULL, false);
		test_payload[0] = 6;
		ipc_msg_send(msg, NULL, false);
		return;
	case MSG_SEND_OVERSIZE:
		msg_set(msg, TEST_HEADER, CONFIG_SOF_IPC_USER_MSG_MAX_SIZE + 1);
		ipc_msg_send(msg, NULL, false);
		break;
	case MSG_SEND_KERNEL_PAYLOAD:
		msg_set(msg, TEST_HEADER, 4);
		msg->tx_data = kernel_only_word;
		ipc_msg_send(msg, NULL, false);
		break;
	case MSG_SEND_MANY:
		for (i = 0; i < TEST_MSGS; i++) {
			msg_set(&test_msgs[i], TEST_HEADER, 0);
			ipc_msg_send(&test_msgs[i], NULL, false);
		}
		return;
	case MSG_REMOVE:
		ipc_msg_list_remove(msg);
		return;
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("syscall security check did not fault");
}

static void run_case(enum msg_test_case test_case, struct k_mem_domain *domain,
		     bool expect_fault)
{
	int ret;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, (void *)(uintptr_t)test_case, NULL, NULL,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_thread(domain, &user_thread);

	if (expect_fault)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);

	sof_boot_test_set_fault_valid(NULL);

	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

/*
 * Count the queued test messages and copy the first one, with up to
 * sizeof(test_payload) bytes of its payload.
 */
static int queued_test_msgs(struct ipc_msg *copy, uint32_t *payload)
{
	struct ipc_kstate *ks = ipc_ks(ipc_get());
	struct list_item *item;
	k_spinlock_key_t key;
	int count = 0;

	key = k_spin_lock(&ks->lock);

	list_for_item(item, &ks->msg_list) {
		struct ipc_msg *msg = container_of(item, struct ipc_msg, list);

		if (msg->header != TEST_HEADER)
			continue;

		if (!count++ && copy) {
			*copy = *msg;
			memcpy(payload, msg->tx_data, MIN(msg->tx_size, sizeof(test_payload)));
		}
	}

	k_spin_unlock(&ks->lock, key);

	return count;
}

static void check_one_queued(uint32_t size, uint32_t first_word)
{
	uint32_t payload[ARRAY_SIZE(test_payload)] = { 0 };
	struct ipc_msg copy;

	zassert_equal(queued_test_msgs(&copy, payload), 1, "expected one queued message");
	zassert_equal(copy.tx_size, size);
	zassert_equal(payload[0], first_word);
	zassert_not_equal(copy.tx_data, test_payload, "kernel queued the user payload");
	zassert_not_equal(copy.callback, planted_callback, "kernel uses the user callback");
}

static void *ipc_msg_queue_setup(void)
{
	struct k_mem_partition *parts[] = { &ipc_msg_test_part };

	zassert_equal(k_mem_domain_init(&domain_a, ARRAY_SIZE(parts), parts), 0);

	domain_b = zephyr_ll_mem_domain();
	zassert_equal(k_mem_domain_add_partition(domain_b, &ipc_msg_test_part), 0);

	return NULL;
}

static void ipc_msg_queue_teardown(void *fixture)
{
	zassert_equal(k_mem_domain_remove_partition(domain_b, &ipc_msg_test_part), 0);
	zassert_equal(k_mem_domain_deinit(&domain_a), 0);
}

static void ipc_msg_queue_after(void *fixture)
{
	ipc_user_msg_purge(&domain_a);
	ipc_user_msg_purge(domain_b);
	zassert_equal(queued_test_msgs(NULL, NULL), 0);
	zassert_equal(kernel_only_word[0], 0, "kernel-only data was written");
}

ZTEST(userspace_ipc_msg_queue, send_copies_once)
{
	uint32_t payload[ARRAY_SIZE(test_payload)] = { 0 };
	struct ipc_msg copy;

	run_case(MSG_SEND_THEN_CHANGE, &domain_a, false);

	zassert_equal(queued_test_msgs(&copy, payload), 1);
	zassert_equal(copy.tx_size, 8);
	zassert_equal(payload[0], 1);
	zassert_equal(payload[1], 2);
	zassert_not_equal(copy.callback, planted_callback);
}

ZTEST(userspace_ipc_msg_queue, forged_list_and_callback_ignored)
{
	run_case(MSG_SEND_FORGED_FIELDS, &domain_a, false);
	check_one_queued(4, 3);
}

ZTEST(userspace_ipc_msg_queue, send_with_data)
{
	run_case(MSG_SEND_DATA, &domain_a, false);
	check_one_queued(4, 4);
}

ZTEST(userspace_ipc_msg_queue, resend_updates_queued_copy)
{
	run_case(MSG_SEND_TWICE, &domain_a, false);
	check_one_queued(4, 6);
}

ZTEST(userspace_ipc_msg_queue, oversize_rejected)
{
	run_case(MSG_SEND_OVERSIZE, &domain_a, true);
	zassert_equal(queued_test_msgs(NULL, NULL), 0);
}

ZTEST(userspace_ipc_msg_queue, kernel_payload_rejected)
{
	run_case(MSG_SEND_KERNEL_PAYLOAD, &domain_a, true);
	zassert_equal(queued_test_msgs(NULL, NULL), 0);
}

ZTEST(userspace_ipc_msg_queue, slots_limit_queue)
{
	run_case(MSG_SEND_MANY, &domain_a, false);
	zassert_equal(queued_test_msgs(NULL, NULL), CONFIG_SOF_IPC_USER_MSG_SLOTS);
}

ZTEST(userspace_ipc_msg_queue, remove_by_owner_only)
{
	run_case(MSG_SEND_FORGED_FIELDS, &domain_a, false);

	/* same message address, other domain: not removed */
	run_case(MSG_REMOVE, domain_b, false);
	zassert_equal(queued_test_msgs(NULL, NULL), 1);

	run_case(MSG_REMOVE, &domain_a, false);
	zassert_equal(queued_test_msgs(NULL, NULL), 0);

	/* removing a message that is not queued is a no-op */
	run_case(MSG_REMOVE, &domain_a, false);
}

ZTEST(userspace_ipc_msg_queue, purge_by_domain)
{
	run_case(MSG_SEND_FORGED_FIELDS, &domain_a, false);
	run_case(MSG_SEND_FORGED_FIELDS, domain_b, false);
	zassert_equal(queued_test_msgs(NULL, NULL), 2);

	ipc_user_msg_purge(&domain_a);
	zassert_equal(queued_test_msgs(NULL, NULL), 1);
}

ZTEST_SUITE(userspace_ipc_msg_queue, sof_boot_test_standalone_predicate, ipc_msg_queue_setup,
	    NULL, ipc_msg_queue_after, ipc_msg_queue_teardown);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_ipc_msg_queue, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);

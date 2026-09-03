// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.

#include <zephyr/ztest.h>
#include <sof/list.h>

/**
 * @brief Test list_init functionality
 *
 * Tests that list.prev and list.next point to the list itself after initialization
 */
ZTEST(sof_list_suite, test_list_init)
{
	struct list_item list = {.prev = NULL, .next = NULL};

	list_init(&list);

	/* Verify that prev and next pointers point to the list itself after initialization */
	zassert_equal(&list, list.prev, "list.prev should point to itself after list_init");
	zassert_equal(&list, list.next, "list.next should point to itself after list_init");
}

/**
 * @brief Test list_is_empty functionality
 *
 * Tests that list_is_empty returns true for empty lists and false for non-empty lists
 */
ZTEST(sof_list_suite, test_list_is_empty)
{
	struct list_item list;
	struct list_item item;

	/* Test when list is empty */
	list_init(&list);
	zassert_true(list_is_empty(&list), "list_is_empty should return true for empty list");

	/* Test when list is not empty */
	list_item_append(&item, &list);
	zassert_false(list_is_empty(&list), "list_is_empty should return false for non-empty list");
}

/**
 * @brief Test list_item_append functionality
 *
 * Tests that list_item_append correctly appends an item to the end of the list
 */
ZTEST(sof_list_suite, test_list_item_append)
{
	struct list_item head;
	struct list_item item1;
	struct list_item item2;

	/* Initialize list */
	list_init(&head);

	/* Append first item */
	list_item_append(&item1, &head);
	zassert_equal(&item1, head.next, "head->next should point to item1");
	zassert_equal(&item1, head.prev, "head->prev should point to item1");
	zassert_equal(&head, item1.next, "item1->next should point to head");
	zassert_equal(&head, item1.prev, "item1->prev should point to head");

	/* Append second item */
	list_item_append(&item2, &head);
	zassert_equal(&item1, head.next, "head->next should still point to item1");
	zassert_equal(&item2, head.prev, "head->prev should now point to item2");
	zassert_equal(&item2, item1.next, "item1->next should now point to item2");
	zassert_equal(&head, item1.prev, "item1->prev should still point to head");
	zassert_equal(&head, item2.next, "item2->next should point to head");
	zassert_equal(&item1, item2.prev, "item2->prev should point to item1");
}

/**
 * @brief Test list_item_prepend functionality
 *
 * Tests that list_item_prepend correctly prepends an item to the beginning of the list
 */
ZTEST(sof_list_suite, test_list_item_prepend)
{
	struct list_item head;
	struct list_item item1;
	struct list_item item2;

	/* Initialize list */
	list_init(&head);

	/* Prepend first item */
	list_item_prepend(&item1, &head);
	zassert_equal(&item1, head.next, "head->next should point to item1");
	zassert_equal(&item1, head.prev, "head->prev should point to item1");
	zassert_equal(&head, item1.next, "item1->next should point to head");
	zassert_equal(&head, item1.prev, "item1->prev should point to head");

	/* Prepend second item */
	list_item_prepend(&item2, &head);
	zassert_equal(&item2, head.next, "head->next should now point to item2");
	zassert_equal(&item1, head.prev, "head->prev should still point to item1");
	zassert_equal(&item1, item2.next, "item2->next should point to item1");
	zassert_equal(&head, item2.prev, "item2->prev should point to head");
	zassert_equal(&head, item1.next, "item1->next should still point to head");
	zassert_equal(&item2, item1.prev, "item1->prev should now point to item2");
}

/**
 * @brief Test list_item_del functionality
 *
 * Tests that list_item_del correctly removes an item from a list
 */
ZTEST(sof_list_suite, test_list_item_del)
{
	struct list_item head;
	struct list_item item1;
	struct list_item item2;

	/* Initialize list */
	list_init(&head);

	/* Add items to list */
	list_item_append(&item1, &head);
	list_item_append(&item2, &head);

	/* Remove first item */
	list_item_del(&item1);

	/* Check that item1 is properly removed and initialized */
	zassert_equal(&item1, item1.next, "item1->next should point to itself after deletion");
	zassert_equal(&item1, item1.prev, "item1->prev should point to itself after deletion");

	/* Check that head and item2 are properly linked */
	zassert_equal(&item2, head.next, "head->next should point to item2");
	zassert_equal(&item2, head.prev, "head->prev should point to item2");
	zassert_equal(&head, item2.next, "item2->next should point to head");
	zassert_equal(&head, item2.prev, "item2->prev should point to head");
}

/**
 * @brief Test list_item_is_last functionality
 *
 * Tests that list_item_is_last correctly identifies the last item in a list
 */
ZTEST(sof_list_suite, test_list_item_is_last)
{
	struct list_item head;
	struct list_item item1;
	struct list_item item2;

	/* Initialize list */
	list_init(&head);

	/* Add items to list */
	list_item_append(&item1, &head);
	list_item_append(&item2, &head);

	/* Check item positions */
	zassert_false(list_item_is_last(&item1, &head),
		      "item1 should not be the last item in the list");
	zassert_true(list_item_is_last(&item2, &head),
		     "item2 should be the last item in the list");
}

/**
 * @brief Test list_relink functionality
 *
 * Tests that list_relink correctly updates references when a list head is moved
 */
ZTEST(sof_list_suite, test_list_relink)
{
	struct list_item old_head;
	struct list_item new_head;
	struct list_item item1;
	struct list_item item2;

	/* Test case 1: Empty list relinking */
	list_init(&old_head);
	new_head = old_head; /* Copy the old head structure */

	list_relink(&new_head, &old_head);

	/* After relinking empty list, new_head should be properly initialized */
	zassert_equal(&new_head, new_head.next,
		      "Empty list: new_head->next should point to itself");
	zassert_equal(&new_head, new_head.prev,
		      "Empty list: new_head->prev should point to itself");

	/* Test case 2: Non-empty list relinking */
	list_init(&old_head);
	list_item_append(&item1, &old_head);
	list_item_append(&item2, &old_head);

	/* Verify initial state - items point to old_head */
	zassert_equal(&old_head, item1.prev, "Initial: item1 prev should point to old_head");
	zassert_equal(&old_head, item2.next, "Initial: item2 next should point to old_head");

	/* Simulate moving list to new location by copying head structure */
	new_head = old_head;
	/* Now new_head.next points to item1, new_head.prev points to item2 */
	/* But item1.prev and item2.next still point to &old_head */

	/* Perform the relinking */
	list_relink(&new_head, &old_head);

	/* After relinking, items should now point to new_head instead of old_head */
	zassert_equal(&new_head, item1.prev, "After relink: item1 prev should point to new_head");
	zassert_equal(&new_head, item2.next, "After relink: item2 next should point to new_head");
	zassert_equal(&item1, new_head.next, "After relink: new_head next should point to item1");
	zassert_equal(&item2, new_head.prev, "After relink: new_head prev should point to item2");

	/* Verify list integrity - items should still be properly linked */
	zassert_equal(&item2, item1.next, "After relink: item1 next should point to item2");
	zassert_equal(&item1, item2.prev, "After relink: item2 prev should point to item1");
}

ZTEST_SUITE(sof_list_suite, NULL, NULL, NULL, NULL, NULL);

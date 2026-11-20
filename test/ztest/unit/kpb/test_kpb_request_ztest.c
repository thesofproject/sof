// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/* Tests of the KPB draining request word, see kpb_request_draining() */

#include <zephyr/ztest.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <errno.h>
#include <stdint.h>

static uint32_t request(void)
{
	return atomic_get(&kpb_drain_request);
}

ZTEST(kpb_request, test_request_fields)
{
	uint32_t before = request();
	uint32_t req;

	zassert_ok(kpb_request_draining(2, 1500));

	req = request();
	zassert_equal(KPB_DRAIN_REQ_SEQ(req), (KPB_DRAIN_REQ_SEQ(before) + 1) & 0xff);
	zassert_equal(KPB_DRAIN_REQ_ID(req), 2);
	zassert_equal(KPB_DRAIN_REQ_LEN(req), 1500);
}

ZTEST(kpb_request, test_request_limits)
{
	uint32_t req;

	zassert_ok(kpb_request_draining(KPB_MAX_NO_OF_CLIENTS - 1, KPB_MAX_DRAINING_REQ));

	req = request();
	zassert_equal(KPB_DRAIN_REQ_ID(req), KPB_MAX_NO_OF_CLIENTS - 1);
	zassert_equal(KPB_DRAIN_REQ_LEN(req), KPB_MAX_DRAINING_REQ);
}

ZTEST(kpb_request, test_invalid_request)
{
	uint32_t before = request();

	zassert_equal(kpb_request_draining(KPB_MAX_NO_OF_CLIENTS, 100), -EINVAL);
	zassert_equal(kpb_request_draining(0, KPB_MAX_DRAINING_REQ + 1), -EINVAL);

	/* a rejected request is not posted */
	zassert_equal(request(), before);
}

ZTEST(kpb_request, test_sequence_wraps)
{
	uint32_t seq = KPB_DRAIN_REQ_SEQ(request());
	uint32_t req;
	int i;

	/* every request changes the sequence number, also when it wraps */
	for (i = 0; i < 300; i++) {
		zassert_ok(kpb_request_draining(i % KPB_MAX_NO_OF_CLIENTS, i));

		req = request();
		seq = (seq + 1) & 0xff;
		zassert_equal(KPB_DRAIN_REQ_SEQ(req), seq);
		zassert_equal(KPB_DRAIN_REQ_ID(req), i % KPB_MAX_NO_OF_CLIENTS);
		zassert_equal(KPB_DRAIN_REQ_LEN(req), i);
	}
}

ZTEST_SUITE(kpb_request, NULL, NULL, NULL, NULL, NULL);

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <sof/alloc.h>
#include <sof/audio/component.h>
#include <mock_trace.h>

TRACE_IMPL()

void *rballoc(int zone, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)caps;

	return malloc(bytes);
}

void *rzalloc(int zone, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)caps;

	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	return 0;
}

void work_schedule_default(struct work *w, uint64_t timeout)
{
}

void notifier_register(struct notifier *notifier)
{
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline)
{
}

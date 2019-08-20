#include <sof/pm_runtime.h>

void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_get_sync()");
}

void pm_runtime_put(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put()");
}

void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put_sync()");
}

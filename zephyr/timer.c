#include <platform/timer.h>
#include <platform/shim.h>

#include <ipc/stream.h>

#include <sof/audio/component.h>

void platform_timer_stop(struct timer *timer)
{
	//TODO
}

void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn)
{
	//TODO
}

void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn)
{
	//TODO
}

void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	//TODO
}

uint64_t platform_timer_get(struct timer *timer)
{
	return (uint64_t)shim_read64(SHIM_DSPWC);
}

int platform_timer_set(struct timer *timer, uint64_t ticks)
{
	/* a tick value of 0 will not generate an IRQ */
	if (ticks == 0)
		ticks = 1;

	/* set new value and run */
	shim_write64(SHIM_DSPWCT0C, ticks);
	shim_write(SHIM_DSPWCTCS, SHIM_DSPWCTCS_T0A);

	return 0;
}

void platform_timer_clear(struct timer *timer)
{
	/* write 1 to clear the timer interrupt */
	shim_write(SHIM_DSPWCTCS, SHIM_DSPWCTCS_T0T);
}

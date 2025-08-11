// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/task.h>
#include <stdint.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <sof/lib/notifier.h>
#include <ipc4/base_fw.h>

#include <zephyr/kernel/thread.h>

LOG_MODULE_REGISTER(dp_schedule, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(dp_sched);

DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active dp tasks */
	struct task ll_tick_src;	/* LL task - source of DP tick */
};

struct task_dp_pdata {
	k_tid_t thread_id;		/* zephyr thread ID */
	struct k_thread thread;		/* memory space for a thread */
	uint32_t deadline_clock_ticks;	/* dp module deadline in Zephyr ticks */
	k_thread_stack_t __sparse_cache *p_stack;	/* pointer to thread stack */
	size_t stack_size;		/* size of the stack in bytes */
	struct k_sem sem;		/* semaphore for task scheduling */
	struct processing_module *mod;	/* the module to be scheduled */
	uint32_t ll_cycles_to_start;    /* current number of LL cycles till delayed start */
};

/* Single CPU-wide lock
 * as each per-core instance if dp-scheduler has separate structures, it is enough to
 * use irq_lock instead of cross-core spinlocks
 */
static inline unsigned int scheduler_dp_lock(void)
{
	return irq_lock();
}

static inline void scheduler_dp_unlock(unsigned int key)
{
	irq_unlock(key);
}

/* dummy LL task - to start LL on secondary cores */
static enum task_state scheduler_dp_ll_tick_dummy(void *data)
{
	return SOF_TASK_STATE_RESCHEDULE;
}

/*
 * DP with Earliest Deadline First scheduling
 *
 * DP a.k.a. "Data processing" is an async scheduling method of data processing modules.
 * Each module works in a separate, preemptible thread with lower priority than LL thread.
 * It allows processing with periods longer than 1ms, on-demand processing, etc.
 *
 * Unlike in LL "low latency" method where a module started every 1ms cycle and all of LL modules
 * together MUST finish processing 1ms, DP works async and gets CPU when a module is "ready for
 * processing", what means:
 *  - on each module's input buffer there's at least IBS bytes of data and in each module's output
 *    buffer there's at least OBS bytes of free space
 *   OR
 *  - a module declared readiness by itself by an optional API call "is_ready_to_process"
 *
 * Critical part is that the module MUST finish processing before its DEADLINE. A deadline is
 * a time when the modules MUST provide a data chunk in order to keep next module(s) in the
 * pipeline working.
 *
 * To ensure that all modules provide data on time - as long as CPU is not overloaded - regardless
 * of modules' processing times and processing periods, a Earliest Deadline First (EDF) scheduling
 * is used.
 * https://en.wikipedia.org/wiki/Earliest_deadline_first_scheduling
 *
 * DEADLINE CALCULATIONS
 * Lets go from the beginning, there are some DEFINITIONS
 *
 * def: buffers' Latest Feeding Time (LFT)
 *	LFT is the latest moment when >>a buffer<< must be fed with a portion of data allowing its
 *	data consumer to work and finish in its specific time
 *
 *    LFT is a parameter specific to a buffer and can be calculated based on:
 *	- current amount of data in the buffer
 *	- data reciever's consumption rate and period
 *	- data source production rate and period
 *	- data reciever's module's LST - latest start time
 *
 *	so LFT in high level is sum of:
 *		- Latest start time (LST) of the data consumer
 *			LST is defined later
 *
 *		- estimated time the consumer will drain the current data from the buffer
 *			number_of_ms_in_buffer / consumer_period
 *			i.e. if there's 5ms of data in the buffer and period of the consumer is
 *			2ms, the calculated time is 4ms
 *
 *		- correction for multiple source cycle
 *		  in case the producer period < consumer period the LFT time needs to be corrected,
 *		  as the producer must process more than once to provide enough data.
 *		  The correction will be calculated as:
 *			producer_LPT * required_number_of_cycles
 *			  where LPT is longest processing time, explained later
 *
 *			correction = producer_LPT *
 *				((consumer_period - number_of_ms_in_buffer) / producer_period)
 *		  if correction is < 0, it should be set to zero
 *		  note that in case producer_period >= consumer_period correction is always 0
 *
 *		finally:
 *			LFT = LST(consumer) + estimatet_drain_time - correction
 *
 *
 * def: DP module's DEADLINE
 *	a DEADLINE is the latest moment >>a module<< must finish processing to feed all target
 *	buffers before their LFTs
 *	Calculation is simple
 *	  - module's deadline is the nearest LFT of all target buffers
 *
 * def: DP module's Longest Processing Time (LPT)
 *	LPT is the longest time the module may process a portion of data, assuming it is scheduled
 *	100% of CPU time
 *	>> LPT cannot be measured in runtime << as processing may change from cycle to cycle, etc.
 *	It can, however, be estimated based on:
 *	 - declared (by a module vendor) number of CPU cycles required for processing.
 *	   This declaration should be done separately for all combination of input/output data
 *	   formats, platform, CPU type, using of HiFi etc.
 *	 - If declaration is not available, we can take "a period" as an approximation of longest
 *	   possible processing time. "A period" is a value calculated using IBS and data consumption
 *	   rate of a module. A module cannot possibly processing longer than its period, because it
 *	   would never provide data in time (if LPT = period that means a module required 100% of
 *	   CPU for processing, so it is really the worst possible case)
 *
 *	   Example: if a data rate is 48samples/msec and OBS = 480samples, the "worst case" period
 *	   should be calculated 10ms
 *
 *      NOTE: in case of sampling freq like 44.1 a round up should taken (like if freq was 45000)
 *
 *	The approximation above, however a correct, is assuming that a module is a heavy one and
 *	it requires 100% of CPU time. Using it may lead to unnecessary buffering, see
 *	"delayed start" section below.
 *
 * def: DP module's latest start time (LST)
 *	LST is the latest moment >>a module<< must begin processing in order to finish within
 *	its deadline.
 *	Calculation: deadline - LPT
 *
 *
 * >>>> Based on an above, it is clear that we do need to calculate first a deadline of the
 *      very latest module in a chain, than go back and calculate LFTs and deadline
 *      of each module separately <<<<
 *
 * Fortunate is that the last module of a pipeline is almost always an LL module (usually DAI).
 * TODO: how to proceed in If there's no LL at the end of pipeline (i.e. in case when the last
 * module is not producing samples - like speech recognition??
 *
 * note that in case if LL1 -> DP1 -> DP3 -> LL2 -> DP3 -> DP4 -> LL2
 * there are 2 separate deadline calculation chains: DP4 than DP3, and (independent) DP2 than DP1
 *
 * also note that deadlines and other parameters may change, so re-calculation of all parameters
 * should occur reasonable frequently and include all DP modules, regardless of a core it is
 * run on

 * for LL module deadline always is "now", so it is very easy to calculate LFTs for
 * its input buffer(s)
 * note: in case of data rates like 44.1, which cannot be divided to 1ms, a round up to 45
 *	 should be used
 *
 *  - LL module always start in 1ms periods
 *  - LL module always consume constant number of bytes in a cycle (with an exception for
 *    frequencies like 44.1, a round up 45KHz should be taken for calculations)
 * so LFT = number of data chunks in buffer * 1ms
 *
 *
 * NOTE!!! "NOW" in all of the calculations is "last start of LL scheduler". It makes all
 * calculations simpler, as in the examples below (calculating CPU cycles would require taking
 * extra care for 32bit overflows or use slow 64bit operations). Also all modules have the same
 * timestamp as "NOW", regardless of moment in the cycle the deadlines are calculated
 *
 * EXAMPLE1 (data source period is longer or equal to data source)
 *  let's take a pipeline:
 *  assuming
 *   - the pipeline is in stable state (processing for a while, not in startup)
 *   - no DP is currently processing
 *   - whole CPU is dedicated to DP, like if LL is on core 0 and DPs on core 1
 *
 *  LL1 ->(buf1, 100ms data) ->  DP1 -> (buf2, 0ms data) -> DP2 -> (buf3, 18ms data) -> LL2
 *				 period 100ms		    period: 20ms
 *				 LPT:  5ms		    LPT: 10ms
 *
 * 1) 0ms time:
 *     - DP1 is ready for processing
 *     - DP2 is not ready for processing
 *    calculate deadlines:
 *     - buf3 LFT = 18ms ==> DP2 deadline = 18ms
 *     - DP2 LST = DP2 deadline - DP2 LPT = 8ms
 *     - buf2 LFT = 8ms ==> DP1 deadline = 8ms
 *
 *    DP1 will be scheduled
 *
 * 2) 5ms time, DP1 has finished processing
 *
 *  LL1 ->(buf1, 5ms data) ->  DP1 -> (buf2, 100ms data) -> DP2 -> (buf3, 13ms data) -> LL2
 *			       period 100ms		    period: 20ms
 *			       LPT:  5ms		    LPT: 10ms
 *
 *     - DP1 is not ready for processing
 *     - DP2 is ready for processing
 *    calculate deadlines:
 *     - buf3 LFT = 13ms ==> DP2 deadline = 13ms
 *     - DP2 LST = 3ms
 *     - buf2 LFT = 5*20ms + 3 = 103ms ==> DP1 deadline = 103ms
 *
 *    DP2 will be scheduled
 *
 *
 *
 *
 * EXAMPLE2 (data source period is shorter than data receiver)
 *
 *  LL1 ->(buf1, 5ms data)  ->  DP1 -> (buf2, 15ms data) -> DP2 -> (buf3, 18ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 * 1) 0ms time
 *   - DP1 is ready for processing
 *   - DP2 is ready for processing
 *
 *    calculate deadlines:
 *     - buf3 LFT = 18ms ==> DP2 deadline = 18ms
 *     - DP2 LST = 8ms
 *     - buf2 LFT = 5 + 8 = 13ms. correction for multiple source cycle is negative => 0
 *		==> DP1 deadline = 13ms
 *
 *    DP1 gets CPU for 2 ms
 *
 * 2) 2ms time
 *  LL1 ->(buf1, 2ms data)  ->  DP1 -> (buf2, 20ms data) -> DP2 -> (buf3, 16ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is not ready for processing
 *   - DP2 is ready for processing
 *
 *    calculate deadlines:
 *     - buf3 LFT = 16ms ==> DP2 deadline = 16ms
 *     - DP2 LST = 6ms
 *     - buf2 LFT = 20ms (1 period) + 6ms = 26ms
 *		==> DP1 deadline = 26ms
 *
 *
 * 3) 12ms time
 *  LL1 ->(buf1, 12ms data) ->  DP1 -> (buf2, 0ms data)  -> DP2 -> (buf3, 24ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines:
 *     - buf3 LFT = 24ms ==> DP2 deadline = 24ms
 *     - DP2 LST = 14ms
 *     - buf2 LFT = 14ms - 4*2 (4 periods * LPT) = 6ms
 *		==> DP1 deadline = 6ms
 *
 *	DP1 gets CPU for 2ms
 *
 * 4) 14ms time
 *  LL1 ->(buf1, 9ms data)  ->  DP1 -> (buf2, 5ms data)  -> DP2 -> (buf3, 22ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines:
 *     - buf3 LFT = 22ms ==> DP2 deadline = 22ms
 *     - DP2 LST = 12ms
 *     - buf2 LFT = 12ms -  correction for multiple source cycle
 *		correction for multiple source cycle = 3*2 (3 periods * LPT) = 6ms
 *     - DP1 deadline = 12ms - 6ms = 6ms
 *
 *	DP1 gets CPU for 2ms
 *
 * 5) 16ms time
 *  LL1 ->(buf1, 6ms data)  ->  DP1 -> (buf2, 10ms data)  -> DP2 -> (buf3, 20ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines:
 *     - buf3 LFT = 20ms ==> DP2 deadline = 20ms
 *     - DP2 LST = 10ms
 *     - buf2 LFT = 10ms - 2*2 (2 periods * LPT) = 6ms
 *		==> DP1 deadline = 6ms
 *
 *	DP1 gets CPU for 2ms
 *
 * 6) 18ms time
 *  LL1 ->(buf1, 3ms data)  ->  DP1 -> (buf2, 15ms data)  -> DP2 -> (buf3, 18ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is not ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines - however pointless at when no DP is ready:
 *     - buf3 LFT = 18ms ==> DP2 deadline = 18ms
 *     - DP2 LST = 8ms
 *     - buf2 LFT = 8ms - 1*2 (1 periods * LPT) = 6ms
 *		==> DP1 deadline = 6ms
 *
 *	no DP is processing for 2 ms
 *
 * 7) 20ms time
 *  LL1 ->(buf1, 5ms data)  ->  DP1 -> (buf2, 15ms data)  -> DP2 -> (buf3, 16ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines -
 *     - buf3 LFT = 16ms ==> DP2 deadline = 16ms
 *     - DP2 LST = 6ms
 *     - buf2 LFT = 6ms - 1*2 (1 periods * LPT) = 4ms
 *		==> DP1 deadline = 4ms
 *
 *	DP1 gets CPU for 2ms
 *
 * 8) 22ms time
 *  LL1 ->(buf1, 2ms data)  ->  DP1 -> (buf2, 20ms data) -> DP2 -> (buf3, 14ms data) -> LL2
 *				period 5ms		    period: 20ms
 *				LPT:  2ms		    LPT: 10ms
 *
 *   - DP1 is ready for processing
 *   - DP2 is not ready for processing
 *
 *    calculate deadlines -
 *     - buf3 LFT = 14ms ==> DP2 deadline = 14ms
 *     - DP2 LST = 4ms
 *     - buf2 LFT = 4ms - 1*2 (1 periods * LPT) = 2ms
 *		==> DP1 deadline = 2ms
 *
 *	DP1 gets CPU for 2ms
 *
 *
 * STARTUP
 * Special case is "pipeline startup". When a pipeline is starting, calculations make no sense,
 * as all the modules are already late and deadlines are in the past
 * To make startup possible DELAYED START mechanism needs to be introduced.
 *
 * Delayed start means that:
 *  - when a DP module becomes ready for a first time, its deadline set to NOW
 *  - even if DP module provides data early, the data will be hold in the buffer
 *    till first LPT passes since DP become ready for the first time
 *
 * The purpose is that a DP may finish processing quicker than its longest processing time. It may
 * be caused by many events, usually by lower CPU load during pipeline startup. If next module
 * starts processing immediately in such situation, it may go into underrun when DP processing take
 * longer in the future. Delaying to declared/estimated LongestProcessingTime (LPT) prevents this,
 * as long as the processing cycles declaration is accurate.
 *
 * Delayed start makes EDF scheduling possible and ensures that even when CPU load close to 100%
 * every module have enough processing time to finish within its deadline.
 *
 *
 * DP SHEDULER
 * A list of all DP tasks, regardless on core the task is on, is to be iterated every time
 * the situation of DP readiness or deadline timing may change, that include
 *  - finish of processing of LL pipeline (on any core)
 *  - finish of processing of any DP module (on any core)
 * TODO
 *
 * during the iteration, the following will be checked:
 *  - Readiness of each DP module
 *	as mentioned before,  module "is ready" when declared readiness by itself an API call
 *	or when it has at least IBS of data on each input and at least OBS free space on each out
 *
 *  - deadline calculation of each DP module
 *    LFTs and Deadlines are not constant, they may change when a module consume/produce
 *    a portion of data. Therefore all LFTs and Deadlines must be re-calculated
 *
 * EXAMPLE
 *
 *  DP1 (20ms period) --(BUF1 10ms data)--> DP2 (10ms period) --(BUF2 3ms data)-->LL (1ms period)
 *  11ms LPT				    1.5ms LPT
 *
 *      current time: 0.1ms before LL cycle
 *
 *  LFT of BUF2: buffer contains 3ms of data, data consumption period is 1ms, so
 *	 LPT(BUF2) = NOW+3.1ms
 *
 *  deadline(DP2) = LFT(BUF2) - LPT(DP2) = NOW + 1.6ms
 *
 * This function checks if the queued DP tasks are ready to processing (meaning
 *    the module run by the task has enough data at all sources and enough free space
 *    on all sinks)
 *
 *    if the task becomes ready, a deadline is set allowing Zephyr to schedule threads
 *    in right order
 *
 * EDF scheduling example
 *
 * example:
 *  Lets assume we do have a pipeline:
 *
 *  LL1 -> DP1 -> LL2 -> DP2 -> LL3 -> DP3 -> LL4
 *
 *  all LLs starts in 1ms tick
 *
 *  for simplification lets assume
 *   - all LLs are on primary core, all DPs on secondary (100% CPU is for DP)
 *   - context switching requires 0 cycles
 *
 *  DP1 - starts every 1ms, needs 0.5ms to finish processing
 *  DP2 - starts every 2ms, needs 0.6ms to finish processing
 *  DP3 - starts every 10ms, needs 0.3ms to finish processing
 *
 * TICK0
 *	only LL1 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *
 * TICK1
 *	LL1 is ready to run
 *	DP1 is ready tu run (has data from LL1) set deadline to TICK2
 *	  LL1 processing (producing second data chunk for DP1)
 *	  DP1 processing for 0.5ms (consuming first data chunk, producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK2
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK3
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK3
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK4
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK4
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK5
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *	!!!!!! Note here - DP1 must do before DP2 as it MUST finish in this tick. DP2 can wait
 *		>>>>>>> this is what we call EDF - EARIEST DEADLINE FIRST <<<<<<
 *
 * TICK5
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK6
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing TWO data chunks for LL3)
 *	CPU is idle for 0.4ms (60% used)
 *
 * TICK6
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK7
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK8
 *	LL3 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		LL3 processing (producing 10% of first data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *
 *
 *   (........ 9 more cycles - LL3 procuces 100% of data for DP3......)
 *
 *
 * TICK15
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK16
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK17
 *	LL3 is ready to run
 *	DP3 is ready to run set deadline to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used -
 *	!!! note that DP3 is ready but has no chance to get CPU in this cycle
 *
 * TICK16
 *	LL1 is ready to run set deadline to TICK17
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK17
 *	LL3 is ready to run
 *	DP3 is in progress, deadline is set to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing data)
 *		DP3 processing for 0.2ms (producing 10 data chunks for LL4)
 *	90% CPU used
 *
 * TICK17
 *	LL1 is ready to run
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is ready to run
 *	LL3 is ready to run
 *	LL4 is ready to run
 *			!! NOTE that DP3 is not ready - it will be ready again in TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing next 10% of second data chunk for DP3)
 *		LL4 processing (consuming 10% of data prepared by DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *		100% CPU used
 *
 *
 * Now - pipeline is in stable state, CPU used almost in 100% (it would be 100% if DP3
 * needed 1.2ms for processing - but the example would be too complicated)
 */
void scheduler_dp_ll_tick(void *receiver_data, enum notify_id event_type, void *caller_data)
{
	(void)receiver_data;
	(void)event_type;
	(void)caller_data;
	struct list_item *tlist;
	struct task *curr_task;
	struct task_dp_pdata *pdata;
	unsigned int lock_key;
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);

	lock_key = scheduler_dp_lock();
	list_for_item(tlist, &dp_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;
		struct processing_module *mod = pdata->mod;

		/* decrease number of LL ticks/cycles left till the module reaches its deadline */
		if (pdata->ll_cycles_to_start) {
			pdata->ll_cycles_to_start--;
			if (!pdata->ll_cycles_to_start)
				/* deadline reached, clear startup delay flag.
				 * see dp_startup_delay comment for details
				 */
				mod->dp_startup_delay = false;
		}

		if (curr_task->state == SOF_TASK_STATE_QUEUED) {
			bool mod_ready;

			mod_ready = module_is_ready_to_process(mod, mod->sources,
							       mod->num_of_sources,
							       mod->sinks,
							       mod->num_of_sinks);
			if (mod_ready) {
				/* set a deadline for given num of ticks, starting now */
				k_thread_deadline_set(pdata->thread_id,
						      pdata->deadline_clock_ticks);

				/* trigger the task */
				curr_task->state = SOF_TASK_STATE_RUNNING;
				k_sem_give(&pdata->sem);
			}
		}
	}
	scheduler_dp_unlock(lock_key);
}

static int scheduler_dp_task_cancel(void *data, struct task *task)
{
	unsigned int lock_key;
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;


	/* this is asyn cancel - mark the task as canceled and remove it from scheduling */
	lock_key = scheduler_dp_lock();

	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	/* if there're no more  DP task, stop LL tick source */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task_cancel(&dp_sch->ll_tick_src);

	/* if the task is waiting on a semaphore - let it run and self-terminate */
	k_sem_give(&pdata->sem);
	scheduler_dp_unlock(lock_key);

	/* wait till the task has finished, if there was any task created */
	if (pdata->thread_id)
		k_thread_join(pdata->thread_id, K_FOREVER);

	return 0;
}

static int scheduler_dp_task_free(void *data, struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;

	scheduler_dp_task_cancel(data, task);

	/* the thread should be terminated at this moment,
	 * abort is safe and will ensure no use after free
	 */
	if (pdata->thread_id) {
		k_thread_abort(pdata->thread_id);
		pdata->thread_id = NULL;
	}

	/* free task stack */
	rfree((__sparse_force void *)pdata->p_stack);
	pdata->p_stack = NULL;

	/* all other memory has been allocated as a single malloc, will be freed later by caller */
	return 0;
}

/* Thread function called in component context, on target core */
static void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	(void)p2;
	(void)p3;
	struct task_dp_pdata *task_pdata = task->priv_data;
	unsigned int lock_key;
	enum task_state state;
	bool task_stop;

	do {
		/*
		 * the thread is started immediately after creation, it will stop on semaphore
		 * Semaphore will be released once the task is ready to process
		 */
		k_sem_take(&task_pdata->sem, K_FOREVER);

		if (task->state == SOF_TASK_STATE_RUNNING)
			state = task_run(task);
		else
			state = task->state;	/* to avoid undefined variable warning */

		lock_key = scheduler_dp_lock();
		/*
		 * check if task is still running, may have been canceled by external call
		 * if not, set the state returned by run procedure
		 */
		if (task->state == SOF_TASK_STATE_RUNNING) {
			task->state = state;
			switch (state) {
			case SOF_TASK_STATE_RESCHEDULE:
				/* mark to reschedule, schedule time is already calculated */
				task->state = SOF_TASK_STATE_QUEUED;
				break;

			case SOF_TASK_STATE_CANCEL:
			case SOF_TASK_STATE_COMPLETED:
				/* remove from scheduling */
				list_item_del(&task->list);
				break;

			default:
				/* illegal state, serious defect, won't happen */
				k_panic();
			}
		}

		/* if true exit the while loop, terminate the thread */
		task_stop = task->state == SOF_TASK_STATE_COMPLETED ||
			task->state == SOF_TASK_STATE_CANCEL;

		scheduler_dp_unlock(lock_key);
	} while (!task_stop);

	/* call task_complete  */
	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);
}

static int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
				     uint64_t period)
{
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;
	unsigned int lock_key;
	uint64_t deadline_clock_ticks;
	int ret;

	lock_key = scheduler_dp_lock();

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		scheduler_dp_unlock(lock_key);
		return -EINVAL;
	}

	/* create a zephyr thread for the task */
	pdata->thread_id = k_thread_create(&pdata->thread, (__sparse_force void *)pdata->p_stack,
					   pdata->stack_size, dp_thread_fn, task, NULL, NULL,
					   CONFIG_DP_THREAD_PRIORITY, K_USER, K_FOREVER);

	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(pdata->thread_id, task->core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto err;
	}

	/* start the thread,  it should immediately stop at a semaphore, so clean it */
	k_sem_reset(&pdata->sem);
	k_thread_start(pdata->thread_id);

	/* if there's no DP tasks scheduled yet, run ll tick source task */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task(&dp_sch->ll_tick_src, 0, 0);

	/* add a task to DP scheduler list */
	task->state = SOF_TASK_STATE_QUEUED;
	list_item_prepend(&task->list, &dp_sch->tasks);

	deadline_clock_ticks = period * CONFIG_SYS_CLOCK_TICKS_PER_SEC;
	/* period/deadline is in us - convert to seconds in next step
	 * or it always will be zero because of integer calculation
	 */
	deadline_clock_ticks /= 1000000;

	pdata->deadline_clock_ticks = deadline_clock_ticks;
	pdata->ll_cycles_to_start = period / LL_TIMER_PERIOD_US;
	pdata->mod->dp_startup_delay = true;
	scheduler_dp_unlock(lock_key);

	tr_dbg(&dp_tr, "DP task scheduled with period %u [us]", (uint32_t)period);
	return 0;

err:
	/* cleanup - unlock and free all allocated resources */
	scheduler_dp_unlock(lock_key);
	k_thread_abort(pdata->thread_id);
	return ret;
}

static struct scheduler_ops schedule_dp_ops = {
	.schedule_task		= scheduler_dp_task_shedule,
	.schedule_task_cancel	= scheduler_dp_task_cancel,
	.schedule_task_free	= scheduler_dp_task_free,
};

int scheduler_dp_init(void)
{
	int ret;
	struct scheduler_dp_data *dp_sch = rzalloc(SOF_MEM_FLAG_KERNEL,
						   sizeof(struct scheduler_dp_data));
	if (!dp_sch)
		return -ENOMEM;

	list_init(&dp_sch->tasks);

	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	/* init src of DP tick */
	ret = schedule_task_init_ll(&dp_sch->ll_tick_src,
				    SOF_UUID(dp_sched_uuid),
				    SOF_SCHEDULE_LL_TIMER,
				    0, scheduler_dp_ll_tick_dummy, dp_sch,
				    cpu_get_id(), 0);

	if (ret)
		return ret;

	notifier_register(NULL, NULL, NOTIFIER_ID_LL_POST_RUN, scheduler_dp_ll_tick, 0);

	return 0;
}

int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size)
{
	void __sparse_cache *p_stack = NULL;

	/* memory allocation helper structure */
	struct {
		struct task task;
		struct task_dp_pdata pdata;
	} *task_memory;

	int ret;

	/* must be called on the same core the task will be binded to */
	assert(cpu_get_id() == core);

	/*
	 * allocate memory
	 * to avoid multiple malloc operations allocate all required memory as a single structure
	 * and return pointer to task_memory->task
	 * As the structure contains zephyr kernel specific data, it must be located in
	 * shared, non cached memory
	 */
	task_memory = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			      sizeof(*task_memory));
	if (!task_memory) {
		tr_err(&dp_tr, "memory alloc failed");
		return -ENOMEM;
	}

	/* allocate stack - must be aligned and cached so a separate alloc */
	stack_size = Z_KERNEL_STACK_SIZE_ADJUST(stack_size);
	p_stack = (__sparse_force void __sparse_cache *)
		rballoc_align(SOF_MEM_FLAG_KERNEL, stack_size, Z_KERNEL_STACK_OBJ_ALIGN);
	if (!p_stack) {
		tr_err(&dp_tr, "stack alloc failed");
		ret = -ENOMEM;
		goto err;
	}

	/* internal SOF task init */
	ret = schedule_task_init(&task_memory->task, uid, SOF_SCHEDULE_DP, 0, ops->run,
				 mod, core, 0);
	if (ret < 0) {
		tr_err(&dp_tr, "schedule_task_init failed");
		goto err;
	}

	/* initialize other task structures */
	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;
	task_memory->task.state = SOF_TASK_STATE_INIT;
	task_memory->task.core = core;

	/* initialize semaprhore */
	k_sem_init(&task_memory->pdata.sem, 0, 1);

	/* success, fill the structures */
	task_memory->task.priv_data = &task_memory->pdata;
	task_memory->pdata.p_stack = p_stack;
	task_memory->pdata.stack_size = stack_size;
	task_memory->pdata.mod = mod;
	*task = &task_memory->task;


	return 0;
err:
	/* cleanup - free all allocated resources */
	rfree((__sparse_force void *)p_stack);
	rfree(task_memory);
	return ret;
}

void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props, uint32_t *data_off_size)
{
	unsigned int lock_key;

	scheduler_props->processing_domain = COMP_PROCESSING_DOMAIN_DP;
	struct scheduler_dp_data *dp_sch =
		(struct scheduler_dp_data *)scheduler_get_data(SOF_SCHEDULE_DP);

	lock_key = scheduler_dp_lock();
	scheduler_get_task_info(scheduler_props, data_off_size,  &dp_sch->tasks);
	scheduler_dp_unlock(lock_key);
}

#include <sof/preproc.h>

#define _TRACE_EVENT_NTH_PARAMS(id_count, param_count)			\
	uintptr_t log_entry						\
	META_SEQ_FROM_0_TO(id_count   , META_SEQ_STEP_id_uint32_t)	\
	META_SEQ_FROM_0_TO(param_count, META_SEQ_STEP_param_uint32_t)

#define _TRACE_EVENT_NTH(postfix, param_count)			\
	META_FUNC_WITH_VARARGS(					\
		_trace_event, META_CONCAT(postfix, param_count),\
		void, _TRACE_EVENT_NTH_PARAMS(2, param_count)	\
	)

#define META_SEQ_STEP_void_param(i, _) (void)META_CONCAT(param,i);

#define _TRACE_N(N, mbox, atomic)					\
	_TRACE_EVENT_NTH(META_CONCAT(META_IF_ELSE(mbox)(_mbox)(),	\
	META_IF_ELSE(atomic)(_atomic)()), N)				\
	{								\
		(void)log_entry;					\
		(void)id_0;						\
		(void)id_1;						\
		META_SEQ_FROM_0_TO(N, META_SEQ_STEP_void_param)		\
	}

#define _TRACE_GROUP(N)		\
	_TRACE_N(N, 0, 0)	\
	_TRACE_N(N, 0, 1)	\
	_TRACE_N(N, 1, 0)	\
	_TRACE_N(N, 1, 1)

#define TRACE_IMPL()	\
	_TRACE_GROUP(0)	\
	_TRACE_GROUP(1)	\
	_TRACE_GROUP(2)	\
	_TRACE_GROUP(3)	\
	_TRACE_GROUP(4)

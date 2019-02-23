#include <sof/audio/kp_common.h>
#include <sof/notifier.h>

/*! Key word detector  runtime data */
struct comp_data {
	struct notify_data notify_data; /**< async notification data */
	struct kp_data data; /**< start,end sample of key word */
};

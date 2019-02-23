#include <sof/notifier.h>
#include <sof/audio/kp_common.h>

/*! Key phrase buffer runtime data */
struct comp_data {
	struct notifier kpb_notifier; /**< async notification data */
	struct kp_data data; /**< start,end sample of key phrase */
};

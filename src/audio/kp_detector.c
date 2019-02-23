#include <sof/audio/kp_detector.h>
#include <sof/audio/component.h>

void kp_dummy_func(void);
struct comp_data dummy_cd;

/* copy and process stream data from source to sink buffers */
static int kp_detector_copy(struct comp_dev *dev)
{
	struct comp_data *cd = &dummy_cd;

	/*...*/
	/* key phrase detected */
	cd->notify_data.id = NOTIFIER_ID_KEY_PHRASE_DETECTED;
	cd->notify_data.data_size = sizeof(struct kp_data);
	cd->notify_data.data = &cd->data;
	/*...*/
	/* triger async notification */
	notifier_event(&cd->notify_data);

	return 0;
}

void kp_dummy_func(void)
{
	kp_detector_copy(NULL);
}


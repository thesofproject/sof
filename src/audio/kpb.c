#include <sof/audio/kpb.h>
#include <sof/audio/component.h>

static struct comp_dev *kpb_new(struct sof_ipc_comp *comp);
static void kpb_set_draining_params(int message, void *cb_data,
				    void *event_data);
void kpb_dummy_func(void);
struct comp_data dummy_cd;
struct comp_dev dummy_dev;

static struct comp_dev *kpb_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev = &dummy_dev;
	struct comp_data *cd = &dummy_cd;

	/* prepare async notifier for key phrase detection */
	cd->kpb_notifier.id = NOTIFIER_ID_KEY_PHRASE_DETECTED;
	cd->kpb_notifier.cb_data = cd;
	cd->kpb_notifier.cb = kpb_set_draining_params;

	/* register KPB for async notification */
	notifier_register(&cd->kpb_notifier);

	return dev;
}

static void kpb_set_draining_params(int message, void *cb_data,
				    void *event_data)
{
	(void)message; /* not used */
	struct comp_data *cd = (struct comp_data *)cb_data;
	struct kp_data *data = (struct kp_data *)event_data;

	/* update sink data */
	cd->data.kp_begin = data->kp_begin;
	cd->data.kp_end = data->kp_end;
}

void kpb_dummy_func(void)
{
	kpb_new(NULL);
	kpb_set_draining_params(0, NULL, NULL);
}

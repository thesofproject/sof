#include <sof/audio/chain_dma.h>
#include <ipc4/error_status.h>
#include <ipc4/module.h>

/* 6a0a274f-27cc-4afb-a3e7-3444723f432e */
DECLARE_SOF_RT_UUID("chain_dma", chain_dma_uuid, 0x6a0a274f, 0x27cc, 0x4afb,
		    0xa3, 0xe7, 0x34, 0x44, 0x72, 0x3f, 0x43, 0x2e);
DECLARE_TR_CTX(chain_dma_tr, SOF_UUID(chain_dma_uuid), LOG_LEVEL_INFO);

int chain_dma_start(struct comp_dev *dev, uint8_t host_dma_id)
{
	return IPC4_SUCCESS;
}

int chain_dma_pause(struct comp_dev *dev, uint8_t host_dma_id)
{
	return IPC4_SUCCESS;
}

int chain_dma_trigger(struct comp_dev *dev, int cmd)
{
	switch (cmd) {
	case COMP_TRIGGER_START:
		chain_dma_start(dev, cmd);
		break;
	case COMP_TRIGGER_PAUSE:
		chain_dma_pause(dev, cmd);
		break;
	default:
		return 0;
	}
    return IPC4_SUCCESS;
}

int chain_dma_remove(struct comp_dev *dev, uint8_t host_dma_id)
{
	return IPC4_SUCCESS;
}

int chain_dma_create(const struct comp_driver *drv, uint8_t host_dma_id, uint8_t link_dma_id, uint32_t fifo_size, bool scs)
{
        struct comp_dev *dev;

        dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
	        return IPC4_INVALID_REQUEST;

        int comp_id = IPC4_COMP_ID(host_dma_id + IPC4_MAX_MODULE_COUNT,
			link_dma_id);

        struct chain_dma_data *cd = comp_get_drvdata(dev);
        size_t size = sizeof(*cd);
        cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	return IPC4_SUCCESS;
}



static const struct comp_driver comp_chain_dma = {
        .type = SOF_COMP_CHAIN_DMA,
        .uid = SOF_RT_UUID(chain_dma_uuid),
        .tctx = &chain_dma_tr,
        .ops = {
                .chain_dma_create = chain_dma_create,
                .trigger = chain_dma_trigger,
                .free = chain_dma_remove,
        },
};

static SHARED_DATA struct comp_driver_info comp_chain_dma_info = {
        .drv = &comp_chain_dma,
};

static void sys_comp_chain_dma_init(void)
{
        comp_register(platform_shared_get(&comp_chain_dma_info,
                                          sizeof(comp_chain_dma_info)));
}

DECLARE_MODULE(sys_comp_chain_dma_init);

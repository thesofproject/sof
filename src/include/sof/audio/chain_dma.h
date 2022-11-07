#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <ipc/dai.h>

static const uint32_t MAX_CHAIN_NUMBER = DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN;

/* chain dma component private data */
struct chain_dma_data {
    bool first_data_received_;
	//! Node id of host HD/A DMA
    uint32_t output_node_id_;
    //! Node id of link HD/A DMA
    uint32_t input_node_id_;
    uint32_t* hw_buffer_;
    bool under_over_run_notification_sent_;
};

int chain_dma_create(const struct comp_driver *drv, uint8_t host_dma_id, uint8_t link_dma_id, uint32_t fifo_size, bool scs);

int chain_dma_start(struct comp_dev *dev, uint8_t host_dma_id);

int chain_dma_pause(struct comp_dev *dev, uint8_t host_dma_id);

int chain_dma_remove(struct comp_dev *dev, uint8_t host_dma_id);

int chain_dma_trigger(struct comp_dev *dev, int cmd);

void SetReadPointer(uint32_t read_pointer);

void SetWritePointer(uint32_t write_pointer);

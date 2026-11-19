// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

/**
 * \file
 * \brief Virtual Zephyr DAI backend for the native_sim (POSIX) build.
 *
 * The native_sim build has no hardware DAI and no signed manifest, so every
 * attempt to instantiate a DAI gateway used to fail early in dai_get(), which
 * made the whole DAI construction and configuration chain unreachable for the
 * fuzzer:
 *
 *     copier_dai_create() -> dai_common_new() -> dai_get() -> dai_get_device()
 *
 * This driver registers a small set of devicetree-described virtual DAI
 * instances so that dai_get_device() finds a match and the generic SOF code
 * (dai_common_new(), comp_dai_config(), dai_set_config(), the property and
 * trigger paths) runs for real.
 *
 * It is deliberately a *configuration-only* model:
 *
 * - The type-specific configuration blob passed to config_set() is accepted
 *   but never parsed. Blob parsing lives in the vendor DAI drivers, and
 *   decoding host-controlled data here would report defects against a device
 *   that does not exist on any product. The blob bytes *are* read once, so
 *   sanitizers can detect reads beyond the backing allocation. This does not
 *   validate the received message length when the blob resides in a larger
 *   IPC buffer; that boundary must be checked by the IPC layer.
 * - config_get() reports a rate and channel count of zero, which SOF reads as
 *   "the hardware accepts any value" (see dai_verify_params()). A virtual DAI
 *   really does accept any stream, so no valid configuration is rejected.
 * - No data is moved. The POSIX DMA model does not implement the transfer
 *   contract yet, so prepare still stops with a clean error there instead of
 *   pretending that audio flowed.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/dai.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT zephyr_posix_dai

/** \brief Synthetic FIFO address of the first virtual DAI instance.
 *
 * The address is only handed to the DMA model as an endpoint identifier, it is
 * never dereferenced. It is non-zero so that code treating a zero FIFO address
 * as "unset" behaves as it does on hardware.
 */
#define POSIX_DAI_FIFO_BASE 0x00010000

/** \brief Address distance between two consecutive virtual FIFOs. */
#define POSIX_DAI_FIFO_STRIDE 0x100

/* Map the devicetree 'dai-type' enum to the Zephyr DAI type. */
#define POSIX_DAI_TYPE_SSP  DAI_INTEL_SSP
#define POSIX_DAI_TYPE_DMIC DAI_INTEL_DMIC
#define POSIX_DAI_TYPE_HDA  DAI_INTEL_HDA
#define POSIX_DAI_TYPE_ALH  DAI_INTEL_ALH
#define POSIX_DAI_TYPE_UAOL DAI_INTEL_UAOL

#define POSIX_DAI_TYPE(inst) \
	_CONCAT(POSIX_DAI_TYPE_, DT_INST_STRING_UPPER_TOKEN(inst, dai_type))

/** \brief Immutable identity of one virtual DAI instance. */
struct posix_dai_cfg {
	enum dai_type type;	/**< Zephyr DAI type reported to SOF. */
	uint32_t dai_index;	/**< Index matched against the gateway node id. */
	uint32_t fifo_address;	/**< Synthetic FIFO address. */
	uint32_t fifo_depth;	/**< FIFO depth reported to SOF. */
};

/** \brief Mutable state of one virtual DAI instance. */
struct posix_dai_data {
	struct k_spinlock lock;		/**< Protects the members below. */
	struct dai_config cfg;		/**< Last configuration accepted. */
	struct dai_properties props;	/**< Scratch for get_properties(). */
	int refcount;			/**< probe()/remove() balance. */
};

/**
 * \brief Sink for the configuration blob read-through.
 *
 * Volatile so the compiler cannot elide the reads that make the sanitizers
 * validate the host-supplied (pointer, size) pair.
 */
static volatile uint8_t posix_dai_blob_sum;

/**
 * \brief Check whether a configuration targets this instance's type.
 *
 * IPC4 passes blob configurations with the NHLT flavour of the type
 * (e.g. DAI_INTEL_SSP_NHLT) while IPC3 passes the base type, see
 * dai_set_config(). Both describe the same link, so accept both.
 *
 * \param[in] type Type of this instance.
 * \param[in] req Type requested by the configuration.
 * \return true when the requested type addresses this instance.
 */
static bool posix_dai_type_matches(enum dai_type type, enum dai_type req)
{
	if (type == req)
		return true;

	switch (type) {
	case DAI_INTEL_SSP:
		return req == DAI_INTEL_SSP_NHLT;
	case DAI_INTEL_DMIC:
		return req == DAI_INTEL_DMIC_NHLT;
	case DAI_INTEL_HDA:
		return req == DAI_INTEL_HDA_NHLT;
	case DAI_INTEL_ALH:
		return req == DAI_INTEL_ALH_NHLT;
	default:
		return false;
	}
}

static int posix_dai_probe(const struct device *dev)
{
	struct posix_dai_data *data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	data->refcount++;
	k_spin_unlock(&data->lock, key);

	return 0;
}

static int posix_dai_remove(const struct device *dev)
{
	struct posix_dai_data *data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	if (data->refcount > 0)
		data->refcount--;

	k_spin_unlock(&data->lock, key);

	return 0;
}

static int posix_dai_config_set(const struct device *dev, const struct dai_config *cfg,
				const void *bespoke_cfg, size_t size)
{
	const struct posix_dai_cfg *dev_cfg = dev->config;
	struct posix_dai_data *data = dev->data;
	k_spinlock_key_t key;

	if (!cfg)
		return -EINVAL;

	if (!posix_dai_type_matches(dev_cfg->type, cfg->type))
		return -EINVAL;

	if (cfg->dai_index != dev_cfg->dai_index)
		return -EINVAL;

	/*
	 * The type-specific blob is intentionally not parsed, see the file
	 * comment. Reading it once still validates the host-controlled
	 * (pointer, size) pair under the sanitizers, which is what a real
	 * driver would do on its first access to the blob.
	 */
	for (size_t i = 0; bespoke_cfg && i < size; i++)
		posix_dai_blob_sum += ((const uint8_t *)bespoke_cfg)[i];

	/* Only the generic part of the configuration is retained. */
	key = k_spin_lock(&data->lock);
	data->cfg = *cfg;
	data->cfg.type = dev_cfg->type;
	k_spin_unlock(&data->lock, key);

	return 0;
}

static int posix_dai_config_get(const struct device *dev, struct dai_config *cfg,
				enum dai_dir dir)
{
	const struct posix_dai_cfg *dev_cfg = dev->config;
	struct posix_dai_data *data = dev->data;
	k_spinlock_key_t key;

	if (!cfg)
		return -EINVAL;

	key = k_spin_lock(&data->lock);
	*cfg = data->cfg;
	k_spin_unlock(&data->lock, key);

	cfg->type = dev_cfg->type;
	cfg->dai_index = dev_cfg->dai_index;

	/*
	 * Zero means "the hardware imposes no value" to the SOF parameter
	 * check, which is what a virtual DAI accepting any stream must report.
	 */
	cfg->rate = 0;
	cfg->channels = 0;

	return 0;
}

static int posix_dai_get_properties_copy(const struct device *dev, enum dai_dir dir,
					 int stream_id, struct dai_properties *prop)
{
	const struct posix_dai_cfg *dev_cfg = dev->config;

	if (!prop || stream_id < 0)
		return -EINVAL;

	prop->fifo_address = dev_cfg->fifo_address;
	prop->fifo_depth = dev_cfg->fifo_depth;
	prop->dma_hs_id = dev_cfg->dai_index;
	prop->reg_init_delay = 0;
	prop->stream_id = dev_cfg->dai_index;

	return 0;
}

static const struct dai_properties *posix_dai_get_properties(const struct device *dev,
							     enum dai_dir dir, int stream_id)
{
	struct posix_dai_data *data = dev->data;
	struct dai_properties props;

	if (posix_dai_get_properties_copy(dev, dir, stream_id, &props) < 0)
		return NULL;

	/*
	 * This API returns a pointer into driver memory, so publish a fully
	 * built copy instead of letting a concurrent query observe a partially
	 * updated object.
	 */
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	data->props = props;
	k_spin_unlock(&data->lock, key);

	return &data->props;
}

static int posix_dai_trigger(const struct device *dev, enum dai_dir dir,
			     enum dai_trigger_cmd cmd)
{
	switch (cmd) {
	case DAI_TRIGGER_START:
	case DAI_TRIGGER_PRE_START:
	case DAI_TRIGGER_STOP:
	case DAI_TRIGGER_PAUSE:
	case DAI_TRIGGER_POST_STOP:
	case DAI_TRIGGER_DRAIN:
	case DAI_TRIGGER_DROP:
	case DAI_TRIGGER_PREPARE:
	case DAI_TRIGGER_RESET:
	case DAI_TRIGGER_COPY:
		return 0;
	default:
		return -EINVAL;
	}
}

/**
 * \brief Accept any timestamping configuration.
 *
 * SOF queries the DAI timestamp interface from the IPC time-info path. The
 * virtual DAI has no counter, so the calls are accepted and report zero.
 *
 * \param[in] dev Virtual DAI instance.
 * \param[in] cfg Timestamp configuration, not interpreted.
 * \return 0 always.
 */
static int posix_dai_ts_config(const struct device *dev, struct dai_ts_cfg *cfg)
{
	return cfg ? 0 : -EINVAL;
}

/** \brief Start timestamping. \see posix_dai_ts_config() */
static int posix_dai_ts_start(const struct device *dev, struct dai_ts_cfg *cfg)
{
	return cfg ? 0 : -EINVAL;
}

/** \brief Stop timestamping. \see posix_dai_ts_config() */
static int posix_dai_ts_stop(const struct device *dev, struct dai_ts_cfg *cfg)
{
	return cfg ? 0 : -EINVAL;
}

/**
 * \brief Return a zeroed timestamp sample.
 *
 * \param[in] dev Virtual DAI instance.
 * \param[in] cfg Timestamp configuration, not interpreted.
 * \param[out] tsd Receives a zeroed sample.
 * \return 0 on success, -EINVAL on a missing argument.
 */
static int posix_dai_ts_get(const struct device *dev, struct dai_ts_cfg *cfg,
			    struct dai_ts_data *tsd)
{
	if (!cfg || !tsd)
		return -EINVAL;

	memset(tsd, 0, sizeof(*tsd));

	return 0;
}

static DEVICE_API(dai, posix_dai_api) = {
	.probe			= posix_dai_probe,
	.remove			= posix_dai_remove,
	.config_set		= posix_dai_config_set,
	.config_get		= posix_dai_config_get,
	.trigger		= posix_dai_trigger,
	.get_properties		= posix_dai_get_properties,
	.get_properties_copy	= posix_dai_get_properties_copy,
	.ts_config		= posix_dai_ts_config,
	.ts_start		= posix_dai_ts_start,
	.ts_stop		= posix_dai_ts_stop,
	.ts_get			= posix_dai_ts_get,
};

#define POSIX_DAI_DEVICE_INIT(inst)							\
	static const struct posix_dai_cfg posix_dai_cfg_##inst = {			\
		.type = POSIX_DAI_TYPE(inst),						\
		.dai_index = DT_INST_PROP(inst, dai_index),				\
		.fifo_address = POSIX_DAI_FIFO_BASE + (inst) * POSIX_DAI_FIFO_STRIDE,	\
		.fifo_depth = DT_INST_PROP(inst, fifo_depth),				\
	};										\
	static struct posix_dai_data posix_dai_data_##inst;				\
											\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL,						\
			      &posix_dai_data_##inst, &posix_dai_cfg_##inst,		\
			      POST_KERNEL, CONFIG_DAI_INIT_PRIORITY, &posix_dai_api);

DT_INST_FOREACH_STATUS_OKAY(POSIX_DAI_DEVICE_INIT)

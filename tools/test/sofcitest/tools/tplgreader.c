#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "asoc.h"

#define SOC_TPLG_PASS_MANIFEST		0
#define SOC_TPLG_PASS_VENDOR		1
#define SOC_TPLG_PASS_MIXER		2
#define SOC_TPLG_PASS_WIDGET		3
#define SOC_TPLG_PASS_PCM_DAI		4
#define SOC_TPLG_PASS_GRAPH		5
#define SOC_TPLG_PASS_PINS		6
#define SOC_TPLG_PASS_BE_DAI		7
#define SOC_TPLG_PASS_LINK		8

#define SOC_TPLG_PASS_START	SOC_TPLG_PASS_MANIFEST
#define SOC_TPLG_PASS_END	SOC_TPLG_PASS_LINK

#define SND_SOC_TPLG_INDEX_ALL  0

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define SNDRV_PCM_FMTBIT_S16_LE  	(1 << 2)
#define SNDRV_PCM_FMTBIT_S24_LE		(1 << 6)
#define SNDRV_PCM_FMTBIT_S32_LE		(1 << 10)
#define SNDRV_PCM_FMTBIT_FLOAT		(1 << 14)

#define DEBUG 0

#if DEBUG
#define dprintf(...)  printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif

static int json_output = 0;
const char* pcm_type_str[] = {
    "capture",
    "playback",
    "both"
};

struct plist {
	void* ptrs[20];
	int n;
};

static void list_init(struct plist *list)
{
	    list->n = 0;
}

static void list_append(struct plist* list, void *ptr)
{
	list->ptrs[list->n++] = ptr;
}

/* topology reader config*/
struct tplgreader {
	const char *in_file;
	const char *out_file;
	FILE *in_fd;
	FILE *out_fd;
};


/* topology reader pcm */
struct tplgreaderpcm {
	const char *pcm_name;
	uint32_t index;
	uint32_t id;
	uint64_t formats;		/* SNDRV_PCM_FMTBIT_* */
	uint32_t rates;			/* SNDRV_PCM_RATE_* */
	uint32_t rate_min;		/* min rate */
	uint32_t rate_max;		/* max rate */
	uint32_t channels_min;		/* min channels */
	uint32_t channels_max;		/* max channels */
	uint32_t sig_bits;		/* number of bits of content */
	uint32_t src;
	uint32_t vol;
};

/* topology context */
struct soc_tplg {
	const char *data;
	size_t size;

	/* runtime FW parsing */
	const char *pos;		/* read postion */
	const char *hdr_pos;	/* header position */
	unsigned int pass;	/* pass number */

	uint32_t index;	/* current block index */
	uint32_t req_index;	/* required index, only loaded/free matching blocks */

	struct plist pcm_list;

#if 0
	/* component caller */
	struct device *dev;
	struct snd_soc_component *comp;

	/* vendor specific kcontrol operations */
	const struct snd_soc_tplg_kcontrol_ops *io_ops;
	int io_ops_count;

	/* vendor specific bytes ext handlers, for TLV bytes controls */
	const struct snd_soc_tplg_bytes_ext_ops *bytes_ext_ops;
	int bytes_ext_ops_count;

	/* optional fw loading callbacks to component drivers */
	struct snd_soc_tplg_ops *ops;
#endif
};

static inline int tplg_is_eof(struct soc_tplg *tplg)
{
	const char *end = tplg->hdr_pos;

	if (end >= tplg->data + tplg->size)
		return 1;
	return 0;
}

static inline unsigned long tplg_get_hdr_offset(struct soc_tplg *tplg)
{
	return (unsigned long)(tplg->hdr_pos - tplg->data);
}

static inline unsigned long tplg_get_offset(struct soc_tplg *tplg)
{
	return (unsigned long)(tplg->pos - tplg->data);
}

static void set_pcm_info(struct tplgreaderpcm *pcm,
	struct snd_soc_tplg_stream_caps *caps)
{
	pcm->pcm_name = strdup(caps->name);
	pcm->channels_min = caps->channels_min;
	pcm->channels_max = caps->channels_max;
	pcm->rates = caps->rates;
	pcm->rate_min = caps->rate_min;
	pcm->rate_max = caps->rate_max;
	pcm->formats = caps->formats;
	pcm->sig_bits = caps->sig_bits;
}

struct tplg_format {
	uint64_t fmt;
	const char *name;
};

static const struct tplg_format tplg_formats[] = {
	{SNDRV_PCM_FMTBIT_S16_LE, "s16le"},
	{SNDRV_PCM_FMTBIT_S24_LE, "s24le"},
	{SNDRV_PCM_FMTBIT_S32_LE, "s32le"},
	{SNDRV_PCM_FMTBIT_FLOAT,  "float"},
};

static const char *find_format(uint64_t format)
{
	int i;

	/* TODO: format is a bit wise field, which means there will be
	 * multi format be supported. Expose all or one?
	 */
	for (i = 0; i < ARRAY_SIZE(tplg_formats); i++) {
		if (tplg_formats[i].fmt & format)
			return tplg_formats[i].name;
	}

	return NULL;
}

static void print_pcm_info(struct tplgreaderpcm *pcm)
{
    if (!json_output) {
        printf("PCM: [%s] ID %d FMT %s RATE_MIN %d RATE MAX %d SRC %d VOL %d\n",
                pcm->pcm_name, pcm->id, find_format(pcm->formats),
                pcm->rate_min, pcm->rate_max, pcm->src, pcm->vol);
    }
}

static int tplgreader_pcm_create(struct soc_tplg *tplg,
	struct snd_soc_tplg_pcm *pcm)
{
	struct snd_soc_tplg_stream_caps *caps;
	struct tplgreaderpcm *trpcm;

  caps = &pcm->caps[SND_SOC_TPLG_STREAM_PLAYBACK];
  if (json_output) {
      printf("{PCM=%s;ID=%d;TYPE=%s;FMT=%s;RATE_MIN=%d;RATE_MAX=%d;}\n",
              pcm->pcm_name,
              pcm->pcm_id,
              pcm_type_str[((pcm->playback << 1) | pcm->capture)-1],
              find_format(caps->formats),
              caps->rate_min, caps->rate_max);
  }

	/* we only want to dump pcm with playback and capture */
	if (pcm->playback && pcm->capture) {
		trpcm = malloc(sizeof(struct tplgreaderpcm));
		if (trpcm == NULL)
			return -ENOMEM;

		list_append(&tplg->pcm_list, trpcm);

		trpcm->id = pcm->pcm_id;
		set_pcm_info(trpcm, caps);
#if DEBUG
		print_pcm_info(trpcm);
#endif
	}

	return 0;
}

#if 0
/* validate header magic, size and type */
static int soc_valid_header(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	if (soc_tplg_get_hdr_offset(tplg) >= tplg->fw->size)
		return 0;

	if (hdr->size != sizeof(*hdr)) {
		dev_err(tplg->dev,
			"ASoC: invalid header size for type %d at offset 0x%lx size 0x%zx.\n",
			hdr->type, soc_tplg_get_hdr_offset(tplg),
			tplg->fw->size);
		return -EINVAL;
	}

	/* big endian firmware objects not supported atm */
	if (hdr->magic == cpu_to_be32(SND_SOC_TPLG_MAGIC)) {
		dev_err(tplg->dev,
			"ASoC: pass %d big endian not supported header got %x at offset 0x%lx size 0x%zx.\n",
			tplg->pass, hdr->magic,
			soc_tplg_get_hdr_offset(tplg), tplg->fw->size);
		return -EINVAL;
	}

	if (hdr->magic != SND_SOC_TPLG_MAGIC) {
		dev_err(tplg->dev,
			"ASoC: pass %d does not have a valid header got %x at offset 0x%lx size 0x%zx.\n",
			tplg->pass, hdr->magic,
			soc_tplg_get_hdr_offset(tplg), tplg->fw->size);
		return -EINVAL;
	}

	/* Support ABI from version 4 */
	if (hdr->abi > SND_SOC_TPLG_ABI_VERSION
		|| hdr->abi < SND_SOC_TPLG_ABI_VERSION_MIN) {
		dev_err(tplg->dev,
			"ASoC: pass %d invalid ABI version got 0x%x need 0x%x at offset 0x%lx size 0x%zx.\n",
			tplg->pass, hdr->abi,
			SND_SOC_TPLG_ABI_VERSION, soc_tplg_get_hdr_offset(tplg),
			tplg->fw->size);
		return -EINVAL;
	}

	if (hdr->payload_size == 0) {
		dev_err(tplg->dev, "ASoC: header has 0 size at offset 0x%lx.\n",
			soc_tplg_get_hdr_offset(tplg));
		return -EINVAL;
	}

	if (tplg->pass == hdr->type)
		dev_dbg(tplg->dev,
			"ASoC: Got 0x%x bytes of type %d version %d vendor %d at pass %d\n",
			hdr->payload_size, hdr->type, hdr->version,
			hdr->vendor_type, tplg->pass);

	return 1;
}
#endif

static int soc_tplg_kcontrol_elems_load(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_ctl_hdr *control_hdr;
	int i;

	if (tplg->pass != SOC_TPLG_PASS_MIXER) {
		tplg->pos += hdr->size + hdr->payload_size;
		return 0;
	}

	dprintf("===============================\n");
	dprintf("KCON: adding %d kcontrols\n", hdr->count);

#if 0
	for (i = 0; i < hdr->count; i++) {

		control_hdr = (struct snd_soc_tplg_ctl_hdr *)tplg->pos;

		if (control_hdr->size != sizeof(*control_hdr)) {
			dev_err(tplg->dev, "ASoC: invalid control size\n");
			return -EINVAL;
		}

		switch (control_hdr->ops.info) {
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_STROBE:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		case SND_SOC_TPLG_CTL_RANGE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
		case SND_SOC_TPLG_DAPM_CTL_PIN:
			soc_tplg_dmixer_create(tplg, 1, hdr->payload_size);
			break;
		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			soc_tplg_denum_create(tplg, 1, hdr->payload_size);
			break;
		case SND_SOC_TPLG_CTL_BYTES:
			soc_tplg_dbytes_create(tplg, 1, hdr->payload_size);
			break;
		default:
			soc_bind_err(tplg, control_hdr, i);
			return -EINVAL;
		}
	}
#endif
	return 0;
}

//TODO
static int soc_tplg_dapm_graph_elems_load(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	//struct snd_soc_dapm_context *dapm = &tplg->comp->dapm;
	//struct snd_soc_dapm_route route;
	struct snd_soc_tplg_dapm_graph_elem *elem;
	struct tplgreaderpcm *pcm = NULL;
	struct tplgreaderpcm *tmp;
	int count = hdr->count, i, j;

	if (tplg->pass != SOC_TPLG_PASS_GRAPH) {
		tplg->pos += hdr->size + hdr->payload_size;
		return 0;
	}

	dprintf("===============================\n");
	dprintf("dapm : index %d adding %d DAPM routes\n", tplg->index, count);
	for (i = 0; i < count; i++) {
		elem = (struct snd_soc_tplg_dapm_graph_elem *)tplg->pos;
		tplg->pos += sizeof(struct snd_soc_tplg_dapm_graph_elem);

		dprintf("route: '%s' -> '%s' -> '%s'\n", elem->source, elem->control, elem->sink);

		/* find if this dapm graph include is part for PCM */
		for ( j = 0; j < tplg->pcm_list.n; j++)
		{
			tmp = (struct tplgreaderpcm *)tplg->pcm_list.ptrs[j];
			if (!strcmp(tmp->pcm_name, elem->source))
				pcm = tmp;
		}


		/* find if PGA or SRC is a part */
		if (pcm && strstr(elem->source, "PGA"))
			pcm->vol = 1;
		if (pcm && strstr(elem->source, "SRC"))
			pcm->src = 1;
	}

	return 0;
}

//TODO
static int soc_tplg_dapm_widget_elems_load(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_dapm_widget *w;
	struct snd_soc_tplg_ctl_hdr *control_hdr;
	struct snd_soc_tplg_enum_control *ec;
	struct snd_soc_tplg_mixer_control *mc;
	struct snd_soc_tplg_bytes_control *be;
	int ret, count = hdr->count, i,j;
	int num_kcontrols;

	if (tplg->pass != SOC_TPLG_PASS_WIDGET)
		return 0;

	dprintf("===============================\n");
	dprintf("widgets : index %d adding %d DAPM widgets\n", tplg->index, count);

	for (j = 0; j < count; j++) {
		w = (struct snd_soc_tplg_dapm_widget *) tplg->pos;

		tplg->pos += (sizeof(struct snd_soc_tplg_dapm_widget) + w->priv.size);

		dprintf("widget : '%s' '%s'\n",
			w->name, w->sname);

		if (w->num_kcontrols == 0) {
			//dprintf("===============================\n");
			//dprintf("\n");
			continue;
		}

		control_hdr = (struct snd_soc_tplg_ctl_hdr *)tplg->pos;
		//dprintf("template %s has %d controls of type %x\n",
			//w->name, w->num_kcontrols, control_hdr->type);
		num_kcontrols = w->num_kcontrols;

		switch (control_hdr->ops.info) {
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_STROBE:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		case SND_SOC_TPLG_CTL_RANGE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
			for (i = 0; i < num_kcontrols; i++) {
				mc = (struct snd_soc_tplg_mixer_control *)tplg->pos;
				tplg->pos += (sizeof(struct snd_soc_tplg_mixer_control) +
					mc->priv.size);
				dprintf("\tmixer control '%s'\n",
					mc->hdr.name);
			}
			break;
		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			for (i = 0; i < num_kcontrols; i++) {
				ec = (struct snd_soc_tplg_enum_control *)tplg->pos;
				tplg->pos += (sizeof(struct snd_soc_tplg_enum_control) +
					ec->priv.size);
				dprintf("\tenum control '%s'\n",
					ec->hdr.name);
			}
			break;
		case SND_SOC_TPLG_CTL_BYTES:
			for (i = 0; i < num_kcontrols; i++) {
				be = (struct snd_soc_tplg_bytes_control *)tplg->pos;
				tplg->pos += (sizeof(struct snd_soc_tplg_bytes_control) +
					be->priv.size);
				dprintf("\tbytes control '%s' with access 0x%x\n",
					be->hdr.name, be->hdr.access);
			}
			break;
		default:
			break;
		}

	}
	
#if 0
	template.reg = w->reg;
	template.shift = w->shift;
	template.mask = w->mask;
	template.subseq = w->subseq;
	template.on_val = w->invert ? 0 : 1;
	template.off_val = w->invert ? 1 : 0;
	template.ignore_suspend = w->ignore_suspend;
	template.event_flags = w->event_flags;
	template.dobj.index = tplg->index;
#endif

#if 0
	switch (control_hdr->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_STROBE:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
		kcontrol_type = SND_SOC_TPLG_TYPE_MIXER;  /* volume mixer */
		template.num_kcontrols = w->num_kcontrols;
		template.kcontrol_news =
			soc_tplg_dapm_widget_dmixer_create(tplg,
			template.num_kcontrols);
		if (!template.kcontrol_news) {
			ret = -ENOMEM;
			goto hdr_err;
		}
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		kcontrol_type = SND_SOC_TPLG_TYPE_ENUM;	/* enumerated mixer */
		template.num_kcontrols = w->num_kcontrols;
		template.kcontrol_news =
			soc_tplg_dapm_widget_denum_create(tplg,
			template.num_kcontrols);
		if (!template.kcontrol_news) {
			ret = -ENOMEM;
			goto hdr_err;
		}
		break;
	case SND_SOC_TPLG_CTL_BYTES:
		kcontrol_type = SND_SOC_TPLG_TYPE_BYTES; /* bytes control */
		template.num_kcontrols = w->num_kcontrols;
		template.kcontrol_news =
			soc_tplg_dapm_widget_dbytes_create(tplg,
				template.num_kcontrols);
		if (!template.kcontrol_news) {
			ret = -ENOMEM;
			goto hdr_err;
		}
		break;
	default:
		dev_err(tplg->dev, "ASoC: invalid widget control type %d:%d:%d\n",
			control_hdr->ops.get, control_hdr->ops.put,
			control_hdr->ops.info);
		ret = -EINVAL;
		goto hdr_err;
	}

widget:
	ret = soc_tplg_widget_load(tplg, &template, w);
	if (ret < 0)
		goto hdr_err;

	/* card dapm mutex is held by the core if we are loading topology
	 * data during sound card init. */
	if (card->instantiated)
		widget = snd_soc_dapm_new_control(dapm, &template);
	else
		widget = snd_soc_dapm_new_control_unlocked(dapm, &template);
	if (IS_ERR(widget)) {
		ret = PTR_ERR(widget);
		/* Do not nag about probe deferrals */
		if (ret != -EPROBE_DEFER)
			dev_err(tplg->dev,
				"ASoC: failed to create widget %s controls (%d)\n",
				w->name, ret);
		goto hdr_err;
	}
	if (widget == NULL) {
		dev_err(tplg->dev, "ASoC: failed to create widget %s controls\n",
			w->name);
		ret = -ENOMEM;
		goto hdr_err;
	}

	widget->dobj.type = SND_SOC_DOBJ_WIDGET;
	widget->dobj.widget.kcontrol_type = kcontrol_type;
	widget->dobj.ops = tplg->ops;
	widget->dobj.index = tplg->index;
	list_add(&widget->dobj.list, &tplg->comp->dobj_list);

	ret = soc_tplg_widget_ready(tplg, widget, w);
	if (ret < 0)
		goto ready_err;



		ret = soc_tplg_dapm_widget_create(tplg, widget);
		if (ret < 0) {
			dev_err(tplg->dev, "ASoC: failed to load widget %s\n",
				widget->name);
			return ret;
		}

	}
#endif

	return 0;
}
static int soc_tplg_pcm_elems_load(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_pcm *pcm, *_pcm;
	int count = hdr->count;
	int ret;
	int i;

	if (tplg->pass != SOC_TPLG_PASS_PCM_DAI)
		return 0;

	dprintf("===============================\n");
	dprintf("PCM: index %d adding %d PCM DAIs\n", tplg->index, count);
	for (i = 0; i < count; i++) {
		pcm = (struct snd_soc_tplg_pcm *)tplg->pos;

		/* create the pcm */
		ret = tplgreader_pcm_create(tplg ,pcm);
		if (ret < 0)
			return ret;

		/* offset by version-specific struct size and
		 * real priv data size
		 */
		tplg->pos += pcm->size + pcm->priv.size;

		/* output log */
		dprintf("PCM: '%s' DAI: '%s'\n",pcm->pcm_name , pcm->dai_name);
	}

#if 0
	/* check the element size and count */
	pcm = (struct snd_soc_tplg_pcm *)tplg->pos;
	if (pcm->size > sizeof(struct snd_soc_tplg_pcm)
		|| pcm->size < sizeof(struct snd_soc_tplg_pcm_v4)) {
		dev_err(tplg->dev, "ASoC: invalid size %d for PCM elems\n",
			pcm->size);
		return -EINVAL;
	}

	if (soc_tplg_check_elem_count(tplg,
		pcm->size, count,
		hdr->payload_size, "PCM DAI")) {
		dev_err(tplg->dev, "ASoC: invalid count %d for PCM DAI elems\n",
			count);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		pcm = (struct snd_soc_tplg_pcm *)tplg->pos;

		/* check ABI version by size, create a new version of pcm
		 * if abi not match.
		 */
		if (pcm->size == sizeof(*pcm)) {
			abi_match = true;
			_pcm = pcm;
		} else {
			abi_match = false;
			pcm_new_ver(tplg, pcm, &_pcm);
		}

		/* create the FE DAIs and DAI links */
		soc_tplg_pcm_create(tplg, _pcm);

		/* offset by version-specific struct size and
		 * real priv data size
		 */
		tplg->pos += pcm->size + _pcm->priv.size;

		if (!abi_match)
			kfree(_pcm); /* free the duplicated one */
	}
#endif

	return 0;
}
/* load physical DAI elements */
static int soc_tplg_dai_elems_load(struct soc_tplg *tplg,
				   struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_dai *dai;
	int count = hdr->count;
	int i;

	if (tplg->pass != SOC_TPLG_PASS_BE_DAI) {
		tplg->pos += hdr->size + hdr->payload_size;
		return 0;
	}

	dprintf("===============================\n");
	dprintf("DAI: index %d adding %d BE DAIs\n", tplg->index, count);

#if 0
	/* config the existing BE DAIs */
	for (i = 0; i < count; i++) {
		dai = (struct snd_soc_tplg_dai *)tplg->pos;
		if (dai->size != sizeof(*dai)) {
			dev_err(tplg->dev, "ASoC: invalid physical DAI size\n");
			return -EINVAL;
		}

		soc_tplg_dai_config(tplg, dai);
		tplg->pos += (sizeof(*dai) + dai->priv.size);
	}
#endif
	return 0;
}

/* Load physical link config elements from the topology context */
static int soc_tplg_link_elems_load(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_link_config *link, *_link;
	int count = hdr->count;
	int i, ret;
	//bool abi_match;

	if (tplg->pass != SOC_TPLG_PASS_LINK) {
		tplg->pos += hdr->size + hdr->payload_size;
		return 0;
	};
	dprintf("===============================\n");
	dprintf("LINK: index %d adding %d links\n", tplg->index, count);

	for (i = 0; i < count; i++) {
		link = (struct snd_soc_tplg_link_config *)tplg->pos;
		tplg->pos += link->size + link->priv.size;
		dprintf("LINK: Name '%s' stream name '%s' id %d\n", link->name,
				link->stream_name, link->id);
	}
#if 0
	/* check the element size and count */
	link = (struct snd_soc_tplg_link_config *)tplg->pos;
	if (link->size > sizeof(struct snd_soc_tplg_link_config)
		|| link->size < sizeof(struct snd_soc_tplg_link_config_v4)) {
		dev_err(tplg->dev, "ASoC: invalid size %d for physical link elems\n",
			link->size);
		return -EINVAL;
	}

	if (soc_tplg_check_elem_count(tplg,
		link->size, count,
		hdr->payload_size, "physical link config")) {
		dev_err(tplg->dev, "ASoC: invalid count %d for physical link elems\n",
			count);
		return -EINVAL;
	}

	/* config physical DAI links */
	for (i = 0; i < count; i++) {
		link = (struct snd_soc_tplg_link_config *)tplg->pos;
		if (link->size == sizeof(*link)) {
			abi_match = true;
			_link = link;
		} else {
			abi_match = false;
			ret = link_new_ver(tplg, link, &_link);
			if (ret < 0)
				return ret;
		}

		ret = soc_tplg_link_config(tplg, _link);
		if (ret < 0)
			return ret;

		/* offset by version-specific struct size and
		 * real priv data size
		 */
		tplg->pos += link->size + _link->priv.size;

		if (!abi_match)
			kfree(_link); /* free the duplicated one */
	}
#endif

	return 0;
}

static int soc_tplg_manifest_load(struct soc_tplg *tplg,
				  struct snd_soc_tplg_hdr *hdr)
{
	struct snd_soc_tplg_manifest *manifest, *_manifest;
	//bool abi_match;
	int err;

	if (tplg->pass != SOC_TPLG_PASS_MANIFEST)
		return 0;

	dprintf("===============================\n");
	dprintf("Mainfest\n");

#if 0
	manifest = (struct snd_soc_tplg_manifest *)tplg->pos;

	/* check ABI version by size, create a new manifest if abi not match */
	if (manifest->size == sizeof(*manifest)) {
		abi_match = true;
		_manifest = manifest;
	} else {
		abi_match = false;
		err = manifest_new_ver(tplg, manifest, &_manifest);
		if (err < 0)
			return err;
	}

	/* pass control to component driver for optional further init */
	if (tplg->comp && tplg->ops && tplg->ops->manifest)
		return tplg->ops->manifest(tplg->comp, tplg->index, _manifest);

	if (!abi_match)	/* free the duplicated one */
		kfree(_manifest);
#endif

	return 0;
}


/* check header type and call appropriate handler */
static int soc_tplg_load_header(struct soc_tplg *tplg,
	struct snd_soc_tplg_hdr *hdr)
{
	tplg->pos = tplg->hdr_pos + sizeof(struct snd_soc_tplg_hdr);

	/* check for matching ID */
	if (hdr->index != tplg->req_index &&
		tplg->req_index != SND_SOC_TPLG_INDEX_ALL)
		return 0;

	tplg->index = hdr->index;

	//fprintf(stdout, "READER: get type %d\n", hdr->type);

	switch (hdr->type) {
	case SND_SOC_TPLG_TYPE_MIXER:
	case SND_SOC_TPLG_TYPE_ENUM:
	case SND_SOC_TPLG_TYPE_BYTES:
		return soc_tplg_kcontrol_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
		return soc_tplg_dapm_graph_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_DAPM_WIDGET:
		return soc_tplg_dapm_widget_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_PCM:
		return soc_tplg_pcm_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_DAI:
		return soc_tplg_dai_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_DAI_LINK:
	case SND_SOC_TPLG_TYPE_BACKEND_LINK:
		/* physical link configurations */
		return soc_tplg_link_elems_load(tplg, hdr);
	case SND_SOC_TPLG_TYPE_MANIFEST:
		return soc_tplg_manifest_load(tplg, hdr);
	default:
		return 0;
	}

	tplg->pos += hdr->size + hdr->payload_size;

	return 0;
}

/* process the topology file headers */
static int tplg_process_headers(struct soc_tplg *tplg)
{
	struct snd_soc_tplg_hdr *hdr;
	int ret;

	tplg->pass = SOC_TPLG_PASS_START;

	/* process the header types from start to end */
	while (tplg->pass <= SOC_TPLG_PASS_END) {

		//printf("PXL DEBUG pass %d\n", tplg->pass);

		tplg->hdr_pos = tplg->data;
		hdr = (struct snd_soc_tplg_hdr *)tplg->hdr_pos;

		while (!tplg_is_eof(tplg)) {

			/* load the header object */
			ret = soc_tplg_load_header(tplg, hdr);
			if (ret < 0)
				return ret;

			//fprintf(stdout, "Reader: read pass %d size %d + %zu at %zu\n", tplg->pass,
					//hdr->payload_size , sizeof(struct snd_soc_tplg_hdr), tplg_get_offset(tplg));

			/* goto next header */
			tplg->hdr_pos += hdr->payload_size +
				sizeof(struct snd_soc_tplg_hdr);
			hdr = (struct snd_soc_tplg_hdr *)tplg->hdr_pos;
		}

		/* next data type pass */
		tplg->pass++;
	}

	return ret;
}

static int read_tplg_file(struct tplgreader *reader)
{
	struct soc_tplg tplg;
	int ret = 0;
	size_t filelen, count;
	char* data;
	int i;
	
	fseek(reader->in_fd, 0L, SEEK_END);
	filelen = ftell(reader->in_fd);
	rewind(reader->in_fd);
	
	data = (char*)malloc(sizeof(char) * filelen);  
	dprintf("Reader: read file size %zu\n", filelen);

	if (data == NULL)  
	{
		fprintf(stderr, "error: unable to alloc mem %zu\n", filelen);
		exit(2);  
	}
	
	count = fread(data, 1, filelen, reader->in_fd);

	if (count != filelen)  
	{
		fprintf(stderr, "error: fail to read file %zu vs %zu n", count, filelen);
		exit(3);  
	}

	tplg.data = data;
	tplg.size = filelen;
	tplg.req_index = SND_SOC_TPLG_INDEX_ALL;
	list_init(&tplg.pcm_list);

	ret = tplg_process_headers(&tplg);

	/* print out all PCM list */

	for ( i = 0; i < tplg.pcm_list.n; i++)
	{
		print_pcm_info(tplg.pcm_list.ptrs[i]);
	}

	free(data);
	return 0;
}
static void usage(char *name)
{
	fprintf(stdout, "%s:\t -i infile -o outfile \n", name);
	exit(0);
}


int main(int argc, char *argv[])
{
	struct tplgreader reader;
	int opt, ret, i, binary = 0;
	reader.in_file = NULL;
	reader.out_file = NULL;

	while ((opt = getopt(argc, argv, "hji:o:")) != -1) {
		switch (opt) {
		case 'o':
			reader.out_file = optarg;
			break;
		case 'i':
			reader.in_file = optarg;
			break;
		case 'j':
			json_output = 1;
			break;
		case 'h':
		default: /* '?' */
			usage(argv[0]);
		}
	}

	//if (reader.in_file == NULL || reader.out_file == NULL)
	if (reader.in_file == NULL)
		usage(argv[0]);

	/* open infile for reading */
	reader.in_fd = fopen(reader.in_file, "r");
	if (reader.in_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			reader.in_file, errno);
	}

	/* open outfile for writing */
	if (reader.out_file) {
		unlink(reader.out_file);
		reader.out_fd = fopen(reader.out_file, "w");
		if (reader.out_fd == NULL) {
			fprintf(stderr, "error: unable to open %s for writing %d\n",
				reader.out_file, errno);
		}
	}

	ret = read_tplg_file(&reader);

	/* close files */
	fclose(reader.in_fd);
	if (reader.out_file)
		fclose(reader.out_fd);

	return ret;
}


#define pr_fmt(fmt)	"virtio-sound: " fmt

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "virtio-snd.h"

static void virtio_snd_reset(void *base)
{
	struct virtio_sound *vdev = base;

	pr_info("reset device\n");
	vdev->ready = false;
	virtio_reset_dev(&vdev->base);
}

/* read configuration layout */
static int virtio_snd_cfgread(void *base, int offset, int size, uint32_t *retval)
{
	struct virtio_sound *vdev = base;
	void *ptr;

	pr_info(">>> %s, offset: 0x%x\n", __func__, offset);
	ptr = ((uint8_t *)&vdev->snd_conf) + offset;
	memcpy(retval, ptr, size);

	return 0;
}

static void virtio_snd_set_status(void *base, uint64_t status)
{
	struct virtio_sound *vdev = base;

	// TODO
	pr_info(">>> %s: status 0x%lu\n", __func__, status);
	if (!!(status & VIRTIO_CONFIG_S_DRIVER_OK))
		vdev->ready = true;
}

/* configuration layout ops */
static struct virtio_ops virtio_snd_ops = {
	.name		= "virtio_sound",
	.nvq		= VIRTIO_SND_VQ_MAX,
	.cfgsize	= sizeof(struct virtio_snd_config),
	.reset		= virtio_snd_reset,
	.cfgread	= virtio_snd_cfgread,
	.cfgwrite	= NULL,
//	.apply_features = virtio_snd_apply_features,
	.set_status	= virtio_snd_set_status,
};

static void virtio_snd_rx_notify(void *vdev, struct virtio_vq_info *vq)
{
	printf("%s callback\n", __func__);
}

static void virtio_snd_tx_notify(void *vdev, struct virtio_vq_info *vq)
{
#if 0
	struct virtio_sound *snd = vdev;
	struct iovec iov;
	uint16_t idx;

	while (vq_has_descs(vq)) {
		if (vq_getchain(vq, &idx, &iov, 1, flags)
	}
#endif
	pr_info(">>> %s\n", __func__);
}

static void virtio_snd_event_notify(void *vdev, struct virtio_vq_info *vq)
{
	printf("%s callback\n", __func__);
}

static struct virtio_snd_pcm_stream *virtio_snd_pcm_get_stream(struct virtio_sound *snd, uint32_t stream)
{
	if (stream >= snd->snd_conf.streams) {
		pr_err("Invalid stream request %d\n", stream);
		return NULL;
	}

	return snd->streams[stream];
}

static uint32_t virtio_snd_handle_pcm_info(struct virtio_sound *snd, struct virtio_snd_command *cmd)
{
	uint32_t code = VIRTIO_SND_S_BAD_MSG;
	struct virtio_snd_pcm_info *info = NULL;
	struct virtio_snd_query_info req;
	struct virtio_snd_pcm_stream *stream;
	size_t count = 0;
	int i = 0;

	for (int i = 0; i < cmd->iovcnt; i++) {
		pr_info(">>> iov[%d]: len %d flags 0x%02x\n", i, cmd->iovs[i].iov_len, cmd->flags[i]);
	}

	memcpy(&req, cmd->iovs[0].iov_base, sizeof(req));

	// TODO
	info = calloc(req.count, sizeof(struct virtio_snd_pcm_info));
	if (info == NULL)
		goto out;

	for (i = 0; i < req.count; i++) {
		stream = virtio_snd_pcm_get_stream(snd, req.start_id+i);
		if (stream == NULL) {
			pr_info("empty stream[%d]\n", i);
			break;
		}

		info[i].hdr.hda_fn_nid = stream->hda_fn_nid;
		info[i].features = stream->features;
		info[i].formats = stream->formats;
		info[i].rates = stream->rates;
		info[i].direction = stream->direction;
		info[i].channels_min = stream->channels_min;
		info[i].channels_max = stream->channels_max;
		count += sizeof(struct virtio_snd_pcm_info);
	}

	pr_info(">>> i %d req.count %d req.size %d count %d\n",
			i, req.count, req.size, count);

	if (i == req.count && req.size >= count) {
		memcpy(cmd->iovs[2].iov_base, info, count);
		cmd->iolen += count;
		code = VIRTIO_SND_S_OK;
	}

	free(info);
out:
	return code;
}

static void virtio_snd_do_ctrl(struct virtio_sound *snd, struct virtio_snd_command *cmd)
{
	virtio_snd_hdr resp = { .code = VIRTIO_SND_S_BAD_MSG };
	struct virtio_snd_query_info *info = cmd->iovs[0].iov_base;

	pr_info(">>> %s\n", __func__);

	switch (info->hdr.code) {
	case VIRTIO_SND_R_PCM_INFO:
		resp.code = virtio_snd_handle_pcm_info(snd, cmd);
		break;
	case VIRTIO_SND_R_PCM_SET_PARAMS:
		break;
	case VIRTIO_SND_R_PCM_PREPARE:
		break;
	case VIRTIO_SND_R_PCM_RELEASE:
		break;
	case VIRTIO_SND_R_PCM_START:
		break;
	case VIRTIO_SND_R_PCM_STOP:
		break;
	default:
		printf("virtio snd header not recognized: 0x%x\n", info->hdr.code);
		break;
	}

	// TODO: kernel probe failed call trace
	pr_info(">>> resp.code 0x%x\n", resp.code);
	memcpy(cmd->iovs[1].iov_base, &resp, sizeof(resp));
	cmd->iolen += sizeof(resp);
}

static void virtio_snd_ctrl_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_sound *snd = vdev;
	struct virtio_snd_command cmd = {0};
	struct iovec iovs[64]; /* one recv, another send */
	uint16_t flags[ARRAY_SIZE(iovs)];
	uint16_t idx;
	int n;

	pr_info(">>> %s: has desc %d\n", __func__, vq_has_descs(vq));
	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iovs, ARRAY_SIZE(iovs), flags);
		if (n < 1) {
			pr_err("vtsnd: %s vq_getchain = %d\n", __func__, n);
			return;
		}

		pr_info(">>> iovcnt %d\n", n);
		cmd.iovcnt = n;
		cmd.iovs = iovs;
		cmd.flags = flags;
		virtio_snd_do_ctrl(snd, &cmd);

		/* iolen is the total length of response */
		vq_relchain(vq, idx, cmd.iolen);
	}

	vq_endchains(vq, 1);
}

static uint32_t virtio_snd_pcm_set_params_impl(struct virtio_sound *s, virtio_snd_pcm_set_params *params)
{
	uint32_t supported_formats, supported_rates;
	uint32_t st = params->hdr.stream_id;

	pr_info(">>>> %s\n", __func__);
	if (st > s->snd_conf.streams || !(s->pcm_params)) {
		pr_err("Streams not initalized\n");
		return VIRTIO_SND_S_BAD_MSG;
	}

	// TODO: from pa

	if (!s->pcm_params[st]) {
		s->pcm_params[st] = calloc(1, sizeof(virtio_snd_pcm_params));
	}
	virtio_snd_pcm_params *st_params = s->pcm_params[st];

	st_params->features = params->features;
	st_params->buffer_bytes = params->buffer_bytes;
	st_params->period_bytes = params->period_bytes;

	if (params->channel < 1 || params->channel > AUDIO_MAX_CHANNELS) {
		pr_err("Number of channels not supported\n");
		return VIRTIO_SND_S_NOT_SUPP;
	}
	st_params->channel = params->channel;
	
	supported_formats = 1 << VIRTIO_SND_PCM_FMT_S8 |
				1 << VIRTIO_SND_PCM_FMT_U8 |
				1 << VIRTIO_SND_PCM_FMT_S16 |
				1 << VIRTIO_SND_PCM_FMT_U16 |
				1 << VIRTIO_SND_PCM_FMT_S32 |
				1 << VIRTIO_SND_PCM_FMT_U32 |
				1 << VIRTIO_SND_PCM_FMT_FLOAT;

	supported_rates = 1 << VIRTIO_SND_PCM_RATE_5512 |
				1 << VIRTIO_SND_PCM_RATE_8000 |
				1 << VIRTIO_SND_PCM_RATE_11025 |
				1 << VIRTIO_SND_PCM_RATE_16000 |
				1 << VIRTIO_SND_PCM_RATE_22050 |
				1 << VIRTIO_SND_PCM_RATE_32000 |
				1 << VIRTIO_SND_PCM_RATE_44100 |
				1 << VIRTIO_SND_PCM_RATE_48000 |
				1 << VIRTIO_SND_PCM_RATE_64000 |
				1 << VIRTIO_SND_PCM_RATE_88200 |
				1 << VIRTIO_SND_PCM_RATE_96000 |
				1 << VIRTIO_SND_PCM_RATE_176399 |
				1 << VIRTIO_SND_PCM_RATE_192000 |
				1 << VIRTIO_SND_PCM_RATE_384000;

	if (!(supported_formats & (1 << params->format))) {
		pr_err("Stream format not supported\n");
		return VIRTIO_SND_S_NOT_SUPP;
	}
	st_params->format = params->format;

	if (!(supported_rates & (1 << params->rate))) {
		pr_err("Stream rate not supported\n");
		return VIRTIO_SND_S_NOT_SUPP;
	}
	st_params->rate = params->rate;

	st_params->period_bytes = params->period_bytes;
	st_params->buffer_bytes = params->buffer_bytes;
	return VIRTIO_SND_S_OK;
}

static struct virtio_snd_pcm_stream *virtio_snd_pcm_prepare_impl(struct virtio_sound *snd, int i)
{
	struct virtio_snd_pcm_stream *st;

	pr_info(">>>> %s\n", __func__);
	st = malloc(sizeof(struct virtio_snd_pcm_stream));
	if (st == NULL)
		return st;

	// TODO
	st->hda_fn_nid = 0;
	st->buffer_bytes = 8192;
	st->period_bytes = 4096;
	st->features = 0;
	st->flags = 0;
	st->direction = VIRTIO_SND_D_OUTPUT;
	st->channels_min = 1;
	st->channels_max = 1;
	st->formats = VIRTIO_SND_D_OUTPUT;
	st->rates = VIRTIO_SND_D_OUTPUT;
	st->tail = -1;
	st->r_pos = 0;
	st->w_pos = 0;

	return st;
}

static int virtio_snd_do_init(struct virtio_sound *snd)
{
	virtio_snd_pcm_set_params default_params = {
		.features = 0,
		.buffer_bytes = 8192,
		.period_bytes = 4096,
		.channel = 1,
		.format = VIRTIO_SND_PCM_FMT_S16,
		.rate = VIRTIO_SND_PCM_RATE_44100,
	};
	int status;

	/* pcm streams */
	// TODO
	snd->streams = calloc(snd->snd_conf.streams, sizeof(virtio_snd_pcm_stream *));
	snd->pcm_params = calloc(snd->snd_conf.streams, sizeof(virtio_snd_pcm_stream *));

	for (int i = 0; i < snd->snd_conf.streams; i++) {
		default_params.hdr.stream_id = i;
		status = virtio_snd_pcm_set_params_impl(snd, &default_params);
		if (status != VIRTIO_SND_S_OK) {
			pr_err("virtio_sound: can't initalize stream params\n");
			goto err;
		}
		snd->streams[i] = virtio_snd_pcm_prepare_impl(snd, i);
		if (snd->streams[i] == NULL) {
			pr_err("virtio_sound: can't prepare streams.\n");
			goto err;
		}
	}

	/* no jack impl */
	return 0;

err:
	// TODO
	return -1;
}

static int virtio_snd_parse(struct virtio_sound *snd, char *opts)
{
	if (strcmp(opts, "pa,") == 0) {
		snd->driver = &pa_driver;
		return snd->driver->parse(snd, opts+3);
	}

	pr_err("%s: unsupport backend type\n");
	return -1;
}

static int virtio_snd_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_sound *virt_sound;
	int retval;

	pr_info(">>>>> %s\n", __func__);

	virt_sound = calloc(1, sizeof(struct virtio_sound));
	if (virt_sound == NULL) {
		pr_err(("virtio_sound: calloc returns NULL\n"));
		return -1;
	}

	/* parse backend driver */
	if (virtio_snd_parse(snd, opts) != 0)
		return -1;

	retval = virtio_snd_do_init(virt_sound);
	if (retval != 0)
		goto err;

	virtio_linkup(&virt_sound->base,
			&virtio_snd_ops,
			virt_sound,
			dev,
			virt_sound->vqs,
			BACKEND_VBSU);

	virt_sound->vqs[VIRTIO_SND_CTRL].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_sound->vqs[VIRTIO_SND_CTRL].notify = virtio_snd_ctrl_notify;
	virt_sound->vqs[VIRTIO_SND_EVENT].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_sound->vqs[VIRTIO_SND_EVENT].notify = virtio_snd_event_notify;
	virt_sound->vqs[VIRTIO_SND_TX].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_sound->vqs[VIRTIO_SND_TX].notify = virtio_snd_tx_notify;
	virt_sound->vqs[VIRTIO_SND_RX].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_sound->vqs[VIRTIO_SND_RX].notify = virtio_snd_rx_notify;

	virt_sound->base.device_caps = (1UL << VIRTIO_F_VERSION_1);

	/* initialize config space */
	pr_info(">>> devid 0x%x virt_type 0x%x\n", VIRTIO_DEV_SOUND, VIRTIO_TYPE_SOUND);
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_SOUND);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_MULTIMEDIA);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_MULTIMEDIA_AUDIO);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_SOUND);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	virtio_set_modern_bar(&virt_sound->base, false);

	retval = virtio_interrupt_init(&virt_sound->base, virtio_uses_msix());
	if (retval != 0)
		goto err;

	virtio_set_io_bar(&virt_sound->base, 0);

	return 0;

err:
	free(virt_sound);
	return retval;
}

static void virtio_snd_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	return;
}

static struct pci_vdev_ops pci_ops_virtio_sound = {
	.class_name	= "virtio-sound",
	.vdev_init	= virtio_snd_init,
	.vdev_deinit	= virtio_snd_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read,
};

DEFINE_PCI_DEVTYPE(pci_ops_virtio_sound);

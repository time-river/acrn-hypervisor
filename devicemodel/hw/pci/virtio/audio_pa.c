#include <pulse/pulseaudio.h>

#include "virtio-snd.h"

static int pa_sample_format_t audfmt_to_pa(int fmt, int endianness)
{
	int format;

	switch(fmt) {
	case VIRTIO_SND_PCM_FMT_U8:
	case VIRTIO_SND_PCM_FMT_S8:
		format = PA_SAMPLE_U8;
		break;
	case VIRTIO_SND_PCM_FMT_S16:
	case VIRTIO_SND_PCM_FMT_U16:
		format = endianness ? PA_SAMPLE_S16BE : PA_SAMPLE_S16LE;
		break;
	case VIRTIO_SND_PCM_FMT_S32:
	case VIRTIO_SND_PCM_FMT_U32
		format = endianness ? PA_SAMPLE_S32BE : PA_SAMPLE_S32LE;
		break;
	default:
		dolog ("Internal logic error: Bad audio format %d\n", afmt);
		format = PA_SAMPLE_U8;
		break;
    	}
	return format;
}

static audfmt_e pa_to_virtfmt(pa_sample_format_t fmt, int *endianness)
{
	switch (fmt) {
	case PA_SAMPLE_U8:
		return VIRTIO_SND_PCM_FMT_U8;
	case PA_SAMPLE_S16BE:
		*endianness = 1;
		return VIRTIO_SND_PCM_FMT_S16;
	case PA_SAMPLE_S16LE:
		*endianness = 0;
		return VIRTIO_SND_PCM_FMT_S16;
	case PA_SAMPLE_S32BE:
		*endianness = 1;
		return VIRTIO_SND_PCM_FMT_S32;
	case PA_SAMPLE_S32LE:
		*endianness = 0;
		return VIRTIO_SND_PCM_FMT_S32;
	default:
		dolog ("Internal logic error: Bad pa_sample_format %d\n", fmt);
		return VIRTIO_SND_PCM_FMT_U8;
	}
}


static void pa_fint(struct virtio_snd *snd)
{
	struct virtsnd_backend *pa = snd->driver;

	snd->driver = NULL;

	if (pa->mainloop)
		pa_threaded_mainloop_stop(pa->mainloop);

	if (pa->context) {
		pa_context_disconnect(pa->context);
		pa_context_unref(pa->context);
	}

	if (pa->mainloop)
		pa_threaded_mainloop_free(pa->mainloop);

	free(pa);
}

static void context_state_cb(pa_context *c, void *opaque)
{
	struct virtsnd_backend *pa = opaque;

	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_READY:
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		pa_threaded_mainloop_signal (g->mainloop, 0);
		break;

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	}
}

static void *pa_init(struct virtio_sound *snd)
{
	struct virtsnd_backend *pa = snd->driver;

	pa->mainloop = pa_thread_mainloop_new();
	if (pa->mainloop == NULL)
		return NULL;

	pa->context = pa_context_new(pa_threaded_mainloop_get_api(pa->mainloop), pa->server);
	if (pa->context == NULL)
		goto out;

	pa_context_set_state_callback(pa->context, context_state_cb, pa);

	if (pa_context_connect(pa->context, pa->conf.server, 0, NULL) < 0)
		goto err;

	pa_threaded_mainloop_lock(pa->mainloop);

	if (pa_threaded_mainloop_start(pa->mainloop) < 0)
		goto unlock_and_err;

	while(true) {
		pa_context_state_t state;

		state = pa_context_get_state(pa->context);
		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state))
			goto unlock_and_err;

		pa_threaded_mainloop_unlock(pa->mainloop);
	}

	return pa;

unlock_and_err:
	pa_threaded_mainloop_wait(pa->mainloop);
err:
	pa_finit(pa);
	return NULL;
}

static int virtio_snd_parse(struct  *snd, char *opts)
{
	struct virtsnd_backend *pa = snd->driver;

	pa->server = "192.168.122.1";
	pa->sink.name = "alsa_output.pci-0000_00_1f.3.hdmi-stereo";
	pa->sink.sample.format = PA_SAMPLE_S16NE;
	pa->sink.sample.channels = 1;
	pa->sink.sample.rate = 44100;

	return 0;
}

static int pa_init(struct virtio_sound *snd)
{
	virtio_snd_config *conf = &snd->snd_conf;
	struct virtsnd_backend *pa = snd->driver;


	// TODO:
	conf->streams = 1; /* one input another output */
	conf->jacks = 0;
	conf->chmaps = 0;

	return 0;
}

static struct audio_pcm_ops pa_pcm_ops = {
	.init_out	= pa_init_out,
	.finit_out	= pa_finit_out,
	.run_out	= pa_run_out,
	.write		= pa_write,
	.ctl_out	= pa_ctl_out,

	.init_in	= pa_init_in,
	.fini_in	= pa_fini_in,
	.read		= pa_read,
	.ctl_in		= pa_ctl_in,
};

struct virtsnd_backend pa_driver = {
	.name	= "pa",
	.desc	= "http://www.pulseaudio.org/",
	.parse	= pa_parse,
	.init	= pa_init,
	.fini	= pa_fini,
};

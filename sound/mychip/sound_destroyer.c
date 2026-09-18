#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>

struct super_chip {
	struct snd_card *card;
	struct snd_pcm_substream *sub;
	struct timer_list timer;
	spinlock_t lock;
	unsigned int bps;
	unsigned int buf_bytes;
	unsigned int period_bytes;
	unsigned int pos;
	bool running;
};

static const struct snd_pcm_hardware super_hw = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_44100,
	.rate_min = 44100,
	.rate_max = 44100,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 128 * 1024,
	.period_bytes_min = 1024,
	.period_bytes_max = 32 * 1024,
	.periods_min = 2,
	.periods_max = 8,
};

static void super_mangle(void *buf, unsigned int bytes)
{
	s16 *s = buf;
	unsigned int n = bytes / sizeof(s16);
	unsigned int i;

	for (i = 0; i < n; i++) {
		s[i] = -s[i];
		if (i & 1)
			s[i] ^= 0x00ff;
	}
}

static void super_timer_cb(struct timer_list *t)
{
	struct super_chip *chip = timer_container_of(chip, t, timer);
	struct snd_pcm_runtime *rt;
	unsigned long flags;
	unsigned int bytes;

	spin_lock_irqsave(&chip->lock, flags);
	if (!chip->running || !chip->sub) {
		spin_unlock_irqrestore(&chip->lock, flags);
		return;
	}

	rt = chip->sub->runtime;
	bytes = chip->period_bytes;

	if (rt->dma_area && bytes)
		super_mangle(rt->dma_area + chip->pos, bytes);

	chip->pos += bytes;
	if (chip->buf_bytes && chip->pos >= chip->buf_bytes)
		chip->pos = 0;

	if (chip->bps && bytes)
		mod_timer(&chip->timer, jiffies + max(1u, HZ * bytes / chip->bps));
	else
		mod_timer(&chip->timer, jiffies + 1);

	spin_unlock_irqrestore(&chip->lock, flags);
	snd_pcm_period_elapsed(chip->sub);
}

static int super_open(struct snd_pcm_substream *sub)
{
	struct super_chip *chip = snd_pcm_substream_chip(sub);

	sub->runtime->hw = super_hw;
	chip->sub = sub;
	return 0;
}

static int super_close(struct snd_pcm_substream *sub)
{
	struct super_chip *chip = snd_pcm_substream_chip(sub);

	chip->sub = NULL;
	return 0;
}

static int super_hw_params(struct snd_pcm_substream *sub,
			   struct snd_pcm_hw_params *params)
{
	return 0;
}

static int super_prepare(struct snd_pcm_substream *sub)
{
	struct super_chip *chip = snd_pcm_substream_chip(sub);
	struct snd_pcm_runtime *rt = sub->runtime;

	chip->buf_bytes = snd_pcm_lib_buffer_bytes(sub);
	chip->period_bytes = snd_pcm_lib_period_bytes(sub);
	chip->bps = rt->rate * rt->channels * 2;
	chip->pos = 0;
	return 0;
}

static int super_trigger(struct snd_pcm_substream *sub, int cmd)
{
	struct super_chip *chip = snd_pcm_substream_chip(sub);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		chip->running = true;
		mod_timer(&chip->timer, jiffies + 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		chip->running = false;
		timer_delete(&chip->timer);
		break;
	default:
		spin_unlock_irqrestore(&chip->lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&chip->lock, flags);
	return 0;
}

static snd_pcm_uframes_t super_pointer(struct snd_pcm_substream *sub)
{
	struct super_chip *chip = snd_pcm_substream_chip(sub);

	return bytes_to_frames(sub->runtime, chip->pos);
}

static const struct snd_pcm_ops super_ops = {
	.open = super_open,
	.close = super_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = super_hw_params,
	.prepare = super_prepare,
	.trigger = super_trigger,
	.pointer = super_pointer,
};

static struct snd_card *super_card;

static int __init super_init(void)
{
	struct super_chip *chip;
	struct snd_pcm *pcm;
	int err;

	err = snd_card_new(NULL, -1, "Nyan", THIS_MODULE,
			   sizeof(*chip), &super_card);
	if (err < 0)
		return err;

	chip = super_card->private_data;
	chip->card = super_card;
	spin_lock_init(&chip->lock);
	timer_setup(&chip->timer, super_timer_cb, 0);

	strcpy(super_card->driver, "Nyan");
	strcpy(super_card->shortname, "Nyan dummy");
	strcpy(super_card->longname, "Nyan dummy PCM (weird)");

	err = snd_pcm_new(super_card, "Nyan PCM", 0, 1, 0, &pcm);
	if (err < 0)
		goto err_card;

	pcm->private_data = chip;
	strcpy(pcm->name, "Nyan");
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &super_ops);
	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
				       NULL, 64 * 1024, 128 * 1024);

	err = snd_card_register(super_card);
	if (err < 0)
		goto err_card;

	pr_info("nyan_pcm: registered, look for card Nyan\n");
	return 0;

err_card:
	snd_card_free(super_card);
	super_card = NULL;
	return err;
}

static void __exit super_exit(void)
{
	if (super_card)
		snd_card_free(super_card);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ANDREW MOOR");
MODULE_DESCRIPTION("Weird Nyan dummy PCM card");
module_init(super_init);
module_exit(super_exit);
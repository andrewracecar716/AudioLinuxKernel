#include <linux/module.h>
#include <linux/init.h>
#include <linux/iiffies.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>

struct super_chip{

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
		SNDRV_PCM_INFO_BLOCK)TRANSFER,
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

static void super_mangle(void *buf,unsigned int bytes)
{
	s16 *s = buf;
	unsigned int n = bytes / sizeof(s16);
	unsigned int i;
	for(i=0;i<n;i++)
	{
		s[i] = -s[i]; //This inverts the buffer of sound
		if(i & 1)
		{
			s[i] ^= 0x00ff; //crushes the right channel
			// what does this do exactly you may ask?
		}
	}

}
static void super_timer_cb(struct timer_list *t)
{
	struct super_chip *chip = from_timer(chip,t,timer);
	stuct snd_pcm_runtime *rt;
	unsigned long flags;
	unsigned int bytes;

	spin_lock_irqsave(&chip->lock,flags);
	if(!chip->running || !chip->sub){
		spin_unlock_irqrestore(&chip->lock,flags);
		return;
	}
	rt = chip->sub->runtime;
	bytes=chip->period_bytes;
	if(rt->dma_area)
		super_mangle(rt->dma_area + chip->pos,bytes);
	chip->pos += bytes;
	if(chip->pos >= chip->buf_bytes)
		

}

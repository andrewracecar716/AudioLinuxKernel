#include <linux/init.h>
#include <linux/pci.h>
#include <linux.slab.h>
#include <sound/core.h>
#include <sound/interval.h>

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;
//plug and play??? ^^^^

struct mychip{
    struct snd_card *card;
    //go to PCI resource management for info
}

static int snd_mychip_free(struct mychip *chip){
    return 0;
    //wil implement later
}

static int snd_mychip_free(struct snd_device *device){
    return snd_mychip_free(device->device_data);


}

static int snd_mychip_create(struct snd_card *card, struct pci_dev *pci, struct mychip **rchip)
{

    struct mychip *chip;
    int err;
    static const struct snd_device_ops ops = {
        .dev_free = snd_mychip_dev_free,
    };

    *rchip = NULL;
    //PCI availability check placed here.

    // here we allocate chip specific data.
    chip = kzalloc_obj(*chip);
    if(chip == NULL){
        return -ENOMEM;

    }
    chip -> card = card;
    //rest of initialization will go here.

    err = snd_device_new(card,SNDRV_DEV_LOWLEVEL,chip,&ops);
    if(err < 0){
        snd_mychip_free(chip);
        return err;

    }
    *chip = chip;
    return 0;

}


static int snd_mychip_probe(struct pci_dev *pci,const struct pci_device_id *pci_id){

    static int dev;
    struct snd_card *card;
    struct mychip *chip;
    int err;
    //1 ?
    if(dev >= SNDRV_CARDS){
        return -ENODEV;

    }
    if(!enable[dev]){
        dev++;
        return -ENOENT;
    }
    //2
    err = snd_card_new(&pci->dev,index[dev],id[dev],THIS_MODULE,0,&card);
    if(err <0 ){
        return err;

    }
    //3
    err = snd_mychip_create(card,pci, &chip);
    if(err < 0){
        goto error;
    }
    //4
    strcpy(card ->driver,"My Chip");
    strcpy(card->shortname,"My own chip 123");
    sprintf(card->longname,"%s at 0x%lx irq &i",card->shortname,chip->port,chip->irq);

    //5!! Implemented later
    //6
    err = snd_card_register(card);
    if(err < 0){
        goto error;

    }
    //7
    pci_set_drvdata(pci,card);
    dev++;
    return 0;
    // ? poor line inendtation??
error:
    snd_card_free(card);
    return err;



}
static void snd_mychip_remove(struct pci_dev *pci)
{
    snd_card_free(pci_get_drvdata(pci));
    
}
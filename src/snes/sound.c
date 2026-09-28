#include <snes.h>
#include "sound.h"
#include "soundbank.h"

/* Banque de sons répartie par smconv sur deux banques ROM (5 et 6). */
extern char SOUNDBANK__0, SOUNDBANK__1;

#define SFX_COUNT 7
#define NO_MUSIC  0xFF

/* Posé par sound_init() : la RAM n'est pas remise à zéro. */
static u8 current;

static u16 music_module(Music track)
{
    switch (track) {
    case MUSIC_OFFER:  return MOD_OFFERTHELIGHT;
    case MUSIC_PURITY: return MOD_PURITY;
    default:           return MOD_ALONELY;
    }
}

void sound_init(void)
{
    current = NO_MUSIC;
    spcBoot();
    /* Banques déclarées dans l'ordre inverse (exemple PVSnesLib
       audio/musicGreaterThan32k). */
    spcSetBank(&SOUNDBANK__1);
    spcSetBank(&SOUNDBANK__0);
}

void sound_music(Music track)
{
    u16 j;
    if (current == (u8)track) return;
    spcStop();
    spcLoad(music_module(track));
    /* Les effets se rechargent après chaque module (exemple PVSnesLib
       audio/effectsandmusic). */
    for (j = 0; j < SFX_COUNT; j++) spcLoadEffect(j);
    spcPlay(0);
    current = (u8)track;
}

void sound_sfx(Sfx effect)
{
    /* Hauteur 2 = 8 kHz, la fréquence des effets ; volume 15, centré. */
    spcEffect(2, (u16)effect, 15 * 16 + 8);
}

void sound_update(void)
{
    spcProcess();
}

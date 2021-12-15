#include "bbb/bbb.h"
#include <stdlib.h>
#include <limits.h>

/* Loose PCM object.
 */
 
void bbb_pcm_del(struct bbb_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}
 
int bbb_pcm_ref(struct bbb_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct bbb_pcm *bbb_pcm_new(int c) {
  if (c<1) return 0;
  if ((int)sizeof(struct bbb_pcm)>INT_MAX-sizeof(int16_t)*c) return 0;
  struct bbb_pcm *pcm=calloc(1,sizeof(struct bbb_pcm)+sizeof(int16_t)*c);
  if (!pcm) return 0;
  
  pcm->refc=1;
  pcm->c=c;
  
  return pcm;
}

/* Fixed-size wave.
 */
 
void bbb_wave_del(struct bbb_wave *wave) {
  if (!wave) return;
  if (wave->refc-->1) return;
  free(wave);
}

int bbb_wave_ref(struct bbb_wave *wave) {
  if (!wave) return -1;
  if (wave->refc<1) return -1;
  if (wave->refc==INT_MAX) return -1;
  wave->refc++;
  return 0;
}

struct bbb_wave *bbb_wave_new() {
  struct bbb_wave *wave=calloc(1,sizeof(struct bbb_wave));
  if (!wave) return 0;
  wave->refc=1;
  return wave;
}

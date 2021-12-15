#include "bbb_context_internal.h"

/* Cleanup.
 */
 
void bbb_voice_cleanup(struct bbb_voice *voice) {
  bbb_pcm_del(voice->pcm);
}

/* Setup.
 */

int bbb_voice_setup(struct bbb_voice *voice,int voiceid,struct bbb_pcm *pcm) {
  if (voiceid<0) return -1;
  if (bbb_pcm_ref(pcm)<0) return -1;
  voice->pcm=pcm;
  if (voiceid&&(pcm->loopz>pcm->loopa)) {
    voice->voiceid=voiceid;
  } else {
    voice->voiceid=0;
  }
  voice->p=0;
  voice->chid=0xff;
  voice->noteid=0xff;
  return 0;
}

/* Update.
 */
 
static inline void bbb_isignal_addv(int16_t *v,int c,const int16_t *a) {
  for (;c-->0;v++,a++) {
    (*v)+=*a;
  }
}

void bbb_voice_update(int16_t *v,int c,struct bbb_voice *voice) {
  if (!voice->pcm) return;
  while (c>0) {
    int cpc;
    if (voice->voiceid) {
      cpc=voice->pcm->loopz-voice->p;
      if (cpc<1) {
        voice->p=voice->pcm->loopa;
        cpc=voice->pcm->loopz-voice->p;
      }
    } else {
      cpc=voice->pcm->c-voice->p;
      if (cpc<1) {
        bbb_pcm_del(voice->pcm);
        voice->pcm=0;
        return;
      }
    }
    if (cpc>c) cpc=c;
    if (cpc<1) return;
    
    bbb_isignal_addv(v,cpc,voice->pcm->v+voice->p);
    voice->p+=cpc;
    v+=cpc;
    c-=cpc;
  }
}

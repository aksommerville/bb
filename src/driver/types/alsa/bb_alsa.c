#include "driver/bb_driver.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define BB_ALSA_BUFFER_SIZE 1024

/* Instance
 */

struct bb_driver_alsa {
  struct bb_driver hdr;

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int hwbuffersize;
  int bufc; // frames
  int bufc_samples;
  int16_t *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort;
  int cberror;
};

#define DRIVER ((struct bb_driver_alsa*)audio)

/* Cleanup.
 */

static void _bb_alsa_del(struct bb_driver *audio) {
  DRIVER->ioabort=1;
  if (DRIVER->iothd&&!DRIVER->cberror) {
    pthread_cancel(DRIVER->iothd);
    pthread_join(DRIVER->iothd,0);
  }
  pthread_mutex_destroy(&DRIVER->iomtx);
  if (DRIVER->hwparams) snd_pcm_hw_params_free(DRIVER->hwparams);
  if (DRIVER->alsa) snd_pcm_close(DRIVER->alsa);
  if (DRIVER->buf) free(DRIVER->buf);
}

/* I/O thread.
 */

static void *bb_alsa_iothd(void *dummy) {
  struct bb_driver *audio=dummy;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);//TODO copied from plundersquad -- is this correct?
  while (1) {
    pthread_testcancel();

    if (pthread_mutex_lock(&DRIVER->iomtx)) {
      DRIVER->cberror=1;
      return 0;
    }
    audio->cb(DRIVER->buf,DRIVER->bufc_samples,audio);
    pthread_mutex_unlock(&DRIVER->iomtx);

    int16_t *samplev=DRIVER->buf;
    int samplep=0,samplec=DRIVER->bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      int err=snd_pcm_writei(DRIVER->alsa,samplev+samplep,samplec-samplep);
      if (DRIVER->ioabort) return 0;
      if (err<=0) {
        if ((err=snd_pcm_recover(DRIVER->alsa,err,0))<0) {
          DRIVER->cberror=1;
          return 0;
        }
        break;
      }
      samplep+=err;
    }
  }
  return 0;
}

/* Init.
 */

static int _bb_alsa_init(struct bb_driver *audio) {
  
  if (
    (snd_pcm_open(&DRIVER->alsa,"default",SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&DRIVER->hwparams)<0)||
    (snd_pcm_hw_params_any(DRIVER->alsa,DRIVER->hwparams)<0)||
    (snd_pcm_hw_params_set_access(DRIVER->alsa,DRIVER->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(DRIVER->alsa,DRIVER->hwparams,SND_PCM_FORMAT_S16)<0)||
    (snd_pcm_hw_params_set_rate_near(DRIVER->alsa,DRIVER->hwparams,&audio->rate,0)<0)||
    (snd_pcm_hw_params_set_channels(DRIVER->alsa,DRIVER->hwparams,audio->chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size(DRIVER->alsa,DRIVER->hwparams,BB_ALSA_BUFFER_SIZE)<0)||
    (snd_pcm_hw_params(DRIVER->alsa,DRIVER->hwparams)<0)
  ) return -1;

  if (snd_pcm_nonblock(DRIVER->alsa,0)<0) return -1;
  if (snd_pcm_prepare(DRIVER->alsa)<0) return -1;

  DRIVER->bufc=BB_ALSA_BUFFER_SIZE;
  DRIVER->bufc_samples=DRIVER->bufc*audio->chanc;
  if (!(DRIVER->buf=malloc(DRIVER->bufc_samples*sizeof(int16_t)))) return -1;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&DRIVER->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&DRIVER->iothd,0,bb_alsa_iothd,audio)) return -1;

  return 0;
}

/* Locks and maintenance.
 */

static int _bb_alsa_update(struct bb_driver *audio) {
  if (DRIVER->cberror) return -1;
  return 0;
}

static int _bb_alsa_lock(struct bb_driver *audio) {
  if (pthread_mutex_lock(&DRIVER->iomtx)) return -1;
  return 0;
}

static int _bb_alsa_unlock(struct bb_driver *audio) {
  pthread_mutex_unlock(&DRIVER->iomtx);
  return 0;
}

/* Type.
 */

static struct bb_driver_alsa _bb_alsa_singleton={0};

const struct bb_driver_type bb_driver_type_alsa={
  .name="alsa",
  .objlen=sizeof(struct bb_driver_alsa),
  .singleton=&_bb_alsa_singleton,
  .del=_bb_alsa_del,
  .init=_bb_alsa_init,
  .update=_bb_alsa_update,
  .lock=_bb_alsa_lock,
  .unlock=_bb_alsa_unlock,
};

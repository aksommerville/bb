#include "bb_pulse_internal.h"

/* I/O thread.
 */
 
static void *bb_pulse_iothd(void *arg) {
  struct bb_driver *driver=arg;
  while (1) {
    if (DRIVER->iocancel) return 0;
    
    // Fill buffer while holding the lock.
    if (pthread_mutex_lock(&DRIVER->iomtx)) {
      DRIVER->ioerror=-1;
      return 0;
    }
    driver->cb(DRIVER->buf,DRIVER->bufa,driver);
    if (pthread_mutex_unlock(&DRIVER->iomtx)) {
      DRIVER->ioerror=-1;
      return 0;
    }
    if (DRIVER->iocancel) return 0;
    
    // Deliver to PulseAudio.
    int err=0,result;
    result=pa_simple_write(DRIVER->pa,DRIVER->buf,sizeof(int16_t)*DRIVER->bufa,&err);
    if (DRIVER->iocancel) return 0;
    if (result<0) {
      DRIVER->ioerror=-1;
      return 0;
    }
  }
}

/* Terminate I/O thread.
 */
 
static void bb_pulse_abort_io(struct bb_driver *driver) {
  if (!DRIVER->iothd) return;
  DRIVER->iocancel=1;
  pthread_join(DRIVER->iothd,0);
  DRIVER->iothd=0;
}

/* Cleanup.
 */
 
static void _bb_pulse_del(struct bb_driver *driver) {
  bb_pulse_abort_io(driver);
  pthread_mutex_destroy(&DRIVER->iomtx);
  if (DRIVER->pa) {
    pa_simple_free(DRIVER->pa);
  }
}

/* Init PulseAudio client.
 */
 
static int bb_pulse_init_pa(struct bb_driver *driver) {
  int err;

  //TODO allow float samples
  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=driver->rate,
    .channels=driver->chanc,
  };
  int bufframec=driver->rate/20; //TODO more sophisticated buffer length decision
  if (bufframec<20) bufframec=20;
  pa_buffer_attr buffer_attr={
    .maxlength=driver->chanc*sizeof(int16_t)*bufframec,
    .tlength=driver->chanc*sizeof(int16_t)*bufframec,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(DRIVER->pa=pa_simple_new(
    0, // server name
    "bb", // our name TODO I'd rather not hard-code this in the driver, can we provide some other way?
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    "bb", // stream (as opposed to client) name
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    return -1;
  }
  
  driver->rate=sample_spec.rate;
  driver->chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int bb_pulse_init_buffer(struct bb_driver *driver) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  DRIVER->bufa=buflen_target_s*driver->rate*driver->chanc;
  if (DRIVER->bufa<buflen_min) {
    DRIVER->bufa=buflen_min;
  } else if (DRIVER->bufa>buflen_max) {
    DRIVER->bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  DRIVER->bufa-=DRIVER->bufa%driver->chanc;
  
  if (!(DRIVER->buf=malloc(sizeof(int16_t)*DRIVER->bufa))) {
    return -1;
  }
  
  return 0;
}

/* Init thread.
 */
 
static int bb_pulse_init_thread(struct bb_driver *driver) {
  int err;
  if (err=pthread_mutex_init(&DRIVER->iomtx,0)) return -1;
  if (err=pthread_create(&DRIVER->iothd,0,bb_pulse_iothd,driver)) return -1;
  return 0;
}

/* Init.
 */
 
static int _bb_pulse_init(struct bb_driver *driver) {
  driver->samplefmt=BB_SAMPLEFMT_SINT16;//TODO
  if (bb_pulse_init_pa(driver)<0) return 0;
  if (bb_pulse_init_buffer(driver)<0) return 0;
  if (bb_pulse_init_thread(driver)<0) return 0;
  return 0;
}

/* Lock.
 */
 
static int _bb_pulse_lock(struct bb_driver *driver) {
  if (pthread_mutex_lock(&DRIVER->iomtx)) return -1;
  return 0;
}

static int _bb_pulse_unlock(struct bb_driver *driver) {
  if (pthread_mutex_unlock(&DRIVER->iomtx)) return -1;
  return 0;
}

/* Type definition.
 */
 
static struct bb_driver_pulse _bb_pulse_singleton={0};

const struct bb_driver_type bb_driver_type_pulse={
  .name="pulse",
  .objlen=sizeof(struct bb_driver_pulse),
  .singleton=&_bb_pulse_singleton,
  .del=_bb_pulse_del,
  .init=_bb_pulse_init,
  .lock=_bb_pulse_lock,
  .unlock=_bb_pulse_unlock,
};

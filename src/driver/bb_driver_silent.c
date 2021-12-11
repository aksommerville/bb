#include "bb_driver.h"
#include <sys/time.h>
#include <stdint.h>

/* Produce no more than N frames of audio per update.
 * If we exceed it, eg by updating too slow, the excess is quietly dropped.
 * This is important, in case the clock behaves unpredictably.
 */
#define BB_SILENT_UPDATE_SANITY_LIMIT 10000

/* Size of our private buffer in samples, and therefore
 * the largest single request we'll send to the callback.
 */
#define BB_SILENT_BUFFER_SIZE 2048

/* Instance definition.
 */
 
struct bb_driver_silent {
  struct bb_driver hdr;
  int64_t prevtime;
  double timescale;
  int16_t buffer[BB_SILENT_BUFFER_SIZE];
};

#define DRIVER ((struct bb_driver_silent*)driver)

/* Time.
 */
 
static int64_t bb_time_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Cleanup.
 */
 
static void _bb_silent_del(struct bb_driver *driver) {
}

/* Init.
 */
 
static int _bb_silent_init(struct bb_driver *driver) {

  //TODO Shouldn't be a big deal to support float too. Only I copied this from an int-only implementation.
  driver->samplefmt=BB_SAMPLEFMT_SINT16;
  
  DRIVER->timescale=(double)driver->rate/1000000.0f;
  
  return 0;
}

/* Update for a given interval in microseconds.
 */
 
static int bb_silent_update_us(struct bb_driver *driver,int64_t us) {

  // Scale and clamp to get the elapsed time in frames.
  int framec=(int)(us*DRIVER->timescale);
  if (framec<1) return 0;
  if (framec>BB_SILENT_UPDATE_SANITY_LIMIT) {
    framec=BB_SILENT_UPDATE_SANITY_LIMIT;
  }
  
  // Generate PCM and throw it away.
  //TODO Would be trivial to record the stream, or analyze eg peak levels. Might be nice for automated testing?
  // If we do that, bear in mind that automated tests might run fast, and collect sound effects too fast.
  // (we run in real time, even if the rest of the app is running hot).
  int samplec=framec*driver->chanc;
  while (samplec>BB_SILENT_BUFFER_SIZE) {
    driver->cb(DRIVER->buffer,BB_SILENT_BUFFER_SIZE,driver);
    framec-=BB_SILENT_BUFFER_SIZE;
  }
  driver->cb(DRIVER->buffer,samplec,driver);
  
  return 0;
}

/* Update.
 */
 
static int _bb_silent_update(struct bb_driver *driver) {

  int64_t now=bb_time_now();
  
  /* First update, just record the time and don't update.
   * App startup can take some time.
   * If we started at init, there would be a huge and pointless first update.
   */
  if (!DRIVER->prevtime) {
    DRIVER->prevtime=now;
    return 0;
  }
  
  int64_t elapsedus=now-DRIVER->prevtime;
  DRIVER->prevtime=now;
  if (bb_silent_update_us(driver,elapsedus)<0) return -1;

  return 0;
}

/* Type definition.
 */
 
struct bb_driver_silent bb_silent_singleton={0};

const struct bb_driver_type bb_driver_type_silent={
  .name="silent",
  .objlen=sizeof(struct bb_driver_silent),
  .singleton=&bb_silent_singleton,
  .del=_bb_silent_del,
  .init=_bb_silent_init,
  .update=_bb_silent_update,
};

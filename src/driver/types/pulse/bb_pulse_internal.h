#ifndef BB_PULSE_INTERNAL_H
#define BB_PULSE_INTERNAL_H

#include "driver/bb_driver.h"
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

struct bb_driver_pulse {
  struct bb_driver hdr;
  
  pa_simple *pa;
  
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int iocancel; // pa_simple doesn't like regular thread cancellation
  
  int16_t *buf;
  int bufa;
};

#define DRIVER ((struct bb_driver_pulse*)driver)

#endif

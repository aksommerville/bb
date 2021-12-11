#ifndef BB_DEMO_H
#define BB_DEMO_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

struct bb_driver;
struct bba_synth;

#define BB_DEFAULT_DEMO_NAME "bba_redline"

// New demo? Add it to this list.
#define BB_FOR_EACH_DEMO \
  _(bba_song) \
  _(bba_redline)

// Invoke this at the bottom of the demo file.
#define BB_DEMO(_name) \
  const struct bb_demo bb_demo_metadata_##_name={ \
    .name=#_name, \
    .quit=demo_##_name##_quit, \
    .init=demo_##_name##_init, \
    .update=demo_##_name##_update, \
    .rate=BB_DEMO_RATE, \
    .chanc=BB_DEMO_CHANC, \
    .driver=BB_DEMO_DRIVER, \
    .synth=BB_DEMO_SYNTH, \
    .report_performance=BB_DEMO_REPORT_PERFORMANCE, \
  };
  
// Redefine these to taste, between including this header and invoking BB_DEMO.
#define BB_DEMO_RATE 44100 /* Main rate, Hertz. */
#define BB_DEMO_CHANC 2 /* Main channel count (1,2) */
#define BB_DEMO_DRIVER 1 /* Nonzero to initialize real output. */
#define BB_DEMO_SYNTH 'a' /* Which synthesizer to initialize: 0,'a' */
#define BB_DEMO_REPORT_PERFORMANCE 1 /* Report CPU usage at teardown. (distracting if you also do it) */

// Globals, initialized for you according to the settings above.
extern struct bb_driver *demo_driver;
extern struct bba_synth demo_bba;

// Real time and CPU time in seconds.
double bb_demo_now();
double bb_demo_cpu_now();
  
/* Private details.
 ****************************************************************/
  
struct bb_demo {
  const char *name;
  void (*quit)();
  int (*init)();
  int (*update)(); // >0 to proceed
  int rate;
  int chanc;
  int driver;
  char synth;
  int report_performance;
};

#define _(name) extern const struct bb_demo bb_demo_metadata_##name;
BB_FOR_EACH_DEMO
#undef _
  
#endif

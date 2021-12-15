#ifndef BB_DEMO_H
#define BB_DEMO_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

struct bb_driver;
struct bb_midi_driver;
struct bba_synth;
struct bbb_context;

#define BB_DEFAULT_DEMO_NAME "bbb_song"

// New demo? Add it to this list.
#define BB_FOR_EACH_DEMO \
  _(bba_song) \
  _(bba_redline) \
  _(bbb_song) \
  _(bbb_redline) \
  _(free_play) \
  _(bbb_pcm_limit)

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
    .bbb_config_path=BB_DEMO_BBB_CONFIG_PATH, \
    .bbb_cache_path=BB_DEMO_BBB_CACHE_PATH, \
    .midi_in=BB_DEMO_MIDI_IN, \
  };
  
// Redefine these to taste, between including this header and invoking BB_DEMO.
#define BB_DEMO_RATE 44100 /* Main rate, Hertz. */
#define BB_DEMO_CHANC 2 /* Main channel count (1,2) */
#define BB_DEMO_DRIVER 1 /* Nonzero to initialize real output. */
#define BB_DEMO_SYNTH 'a' /* Which synthesizer to initialize: 0,'a','b' */
#define BB_DEMO_REPORT_PERFORMANCE 1 /* Report CPU usage at teardown. (distracting if you also do it) */
#define BB_DEMO_BBB_CONFIG_PATH BB_MIDDIR"/demo/data/bbbar-001.bbbar"
#define BB_DEMO_BBB_CACHE_PATH 0 /* Null by default, I expect most demos want a clean state. */
#define BB_DEMO_MIDI_IN 1

// Globals, initialized for you according to the settings above.
extern struct bb_driver *demo_driver;
extern struct bba_synth demo_bba;
extern struct bbb_context *demo_bbb;
extern struct bb_midi_driver *demo_midi_driver;

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
  const char *bbb_config_path;
  const char *bbb_cache_path;
  int midi_in;
};

#define _(name) extern const struct bb_demo bb_demo_metadata_##name;
BB_FOR_EACH_DEMO
#undef _
  
#endif

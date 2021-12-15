#include "bb_demo.h"
#include "share/bb_midi.h"
#include "bbb/bbb.h"

#undef BB_DEMO_SYNTH
#define BB_DEMO_SYNTH 'b'

static void demo_free_play_quit() {
}

static int demo_free_play_init() {

  if (1) { // Force a given pid.
    struct bb_midi_event event={
      .a=0x00,
      .chid=0,
      .opcode=BB_MIDI_OPCODE_PROGRAM,
    };
    int chid=0; for (;chid<16;chid++) {
      event.chid=chid;
      if (bbb_context_event(demo_bbb,&event)<0) return -1;
    }
  }

  return 0;
}

static int demo_free_play_update() {
  return 1;
}

BB_DEMO(free_play)

#include "bb_demo.h"
#include "bbb/bbb.h"
#include "share/bb_midi.h"

#undef BB_DEMO_SYNTH
#define BB_DEMO_SYNTH 'b'

#undef BB_DEMO_DRIVER
#define BB_DEMO_DRIVER 0

#undef BB_DEMO_MIDI_IN
#define BB_DEMO_MIDI_IN 0

#undef BB_DEMO_REPORT_PERFORMANCE
#define BB_DEMO_REPORT_PERFORMANCE 0

static void demo_bbb_pcm_limit_quit() {
}

static int demo_bbb_pcm_limit_init() {

  fprintf(stderr,"Attempting to expose BBB's live PCM limit...\n");
  fprintf(stderr,"TODO: This ought to be an integration test, but at the moment I can only do demos.\n");
  
  // Send 2 million events: Every MIDI-addressable combination of pid, noteid, and velocity.
  // We're not generating any signal yet, so what should happen is every possible sound gets queued for playback.
  int failpid=-1,failnoteid=-1,failvelocity=-1;
  struct bb_midi_event event={0};
  int pid=0; for (;pid<0x80;pid++) {
    event.opcode=BB_MIDI_OPCODE_PROGRAM;
    event.a=pid;
    if (bbb_context_event(demo_bbb,&event)<0) {
      // This shouldn't fail. Changing programs is just replacing one integer in the context state.
      fprintf(stderr,"ERROR: Program 0x%02x\n",pid);
      return -1;
    }
    int noteid=0; for (;noteid<0x80;noteid++) {
      event.opcode=BB_MIDI_OPCODE_NOTE_ON;
      event.a=noteid;
      int velocity=0; for (;velocity<0x80;velocity++) {
        event.b=velocity;
        if (bbb_context_event(demo_bbb,&event)<0) {
          // This should fail after 128 PCMs get queued for playback.
          // We don't know which events actually generate PCM, so we can't assert exactly when it should happen.
          fprintf(stderr,"ERROR: Note 0x%02x:0x%02x:0x%02x\n",pid,noteid,velocity);
          failpid=pid;
          failnoteid=noteid;
          failvelocity=velocity;
          goto _end_of_events_;
        }
      }
    }
  }
 _end_of_events_:;
  if (failpid<0) {
    fprintf(stderr,"Sent 2 million Note-Ons without an error. Either the instrument set is empty, or the PCM limit is broken.\n");
    return -1;
  }
  
  // Generate and discard five seconds of audio. Should be enough to finish all the queued PCMs.
  // But in fact, we only need one of them to finish.
  int16_t signal[1024];
  int samplec=5*bbb_context_get_rate(demo_bbb)*bbb_context_get_chanc(demo_bbb);
  while (samplec>0) {
    bbb_context_update(signal,1024,demo_bbb);
    samplec-=1024;
  }
  
  // Try the Note-On event that failed one more time: Now it should work.
  event.opcode=BB_MIDI_OPCODE_NOTE_ON;
  event.chid=0;
  event.a=failnoteid;
  event.b=failvelocity;
  if (bbb_context_event(demo_bbb,&event)<0) {
    fprintf(stderr,"Retrying event that failed due to PCM limit, retry also failed. Something else is broken.\n");
    return -1;
  }
  
  fprintf(stderr,"bbb_pcm_limit: Success.\n");

  return 0;
}

static int demo_bbb_pcm_limit_update() {
  return 0;
}

BB_DEMO(bbb_pcm_limit)

#include "bb_demo.h"
#include "bba/bba.h"
#include "share/bb_fs.h"

/* On greyskull, I'm seeing scores typically >1500x, and occasionally >2000x.
 * Looking good.
 */

#define SONG_PATH BB_MIDDIR"/demo/data/song/001-anitra.bba"
//#define SONG_PATH BB_MIDDIR"/demo/data/song/002-rach2.bba"
//#define SONG_PATH BB_MIDDIR"/demo/data/song/004-maple.bba"
//#define SONG_PATH BB_MIDDIR"/demo/data/song/005-enigma.bba"

// Rate is interesting; the other demo props should be constant.
#undef BB_DEMO_RATE
#define BB_DEMO_RATE 44100

#undef BB_DEMO_DRIVER
#define BB_DEMO_DRIVER 0

#undef BB_DEMO_SYNTH
#define BB_DEMO_SYNTH 'a'

#undef BB_DEMO_CHANC
#define BB_DEMO_CHANC 1

#undef BB_DEMO_REPORT_PERFORMANCE
#define BB_DEMO_REPORT_PERFORMANCE 0

#undef BB_DEMO_MIDI_IN
#define BB_DEMO_MIDI_IN 0

static void demo_bba_redline_quit() {
}

static int demo_bba_redline_init() {

  if (!demo_bba.mainrate) {
    fprintf(stderr,"bba not initialized\n");
    return -1;
  }

  void *src=0;
  int srcc=bb_file_read(&src,SONG_PATH);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",SONG_PATH);
    return -1;
  }
  
  bba_synth_play_song(&demo_bba,src,srcc);
  demo_bba.songrepeat=0;
  double starttime_real=bb_demo_now();
  double starttime_cpu=bb_demo_cpu_now();
  int framec=0;
  
  while (demo_bba.song) {
    int16_t sample=bba_synth_update(&demo_bba);
    framec++;
    if (framec>0x10000000) {
      fprintf(stderr,"!!! PANIC !!! Running too long, abort.\n");
      break;
    }
  }
  
  if (framec>0) {
    double produced=(double)framec/(double)demo_bba.mainrate;
    double time_real=bb_demo_now()-starttime_real;
    double time_cpu=bb_demo_cpu_now()-starttime_cpu;
    double score=time_real/produced;
    double consumption=time_cpu/time_real;
    fprintf(stderr,
      "%s: rate=%d framec=%d len=%.03fs real=%.03fs cpu=%.03fs score=%.06f [%.0fx]\n",
      SONG_PATH,demo_bba.mainrate,framec,produced,
      time_real,time_cpu,score,1.0/score
    );
  } else {
    fprintf(stderr,"%s: No signal produced.\n",SONG_PATH);
  }
  
  free(src);
  return 0;
}

static int demo_bba_redline_update() {
  return 0;
}

BB_DEMO(bba_redline)

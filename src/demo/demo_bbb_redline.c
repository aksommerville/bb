#include "bb_demo.h"
#include "bbb/bbb.h"
#include "share/bb_fs.h"
#include "share/bb_midi.h"
#include <sys/resource.h>

/* I'm getting truly insane performance scores on greyskull: 4000x..8000x
 * This is only with square and saw programs. Likely to get a lot worse with better ones.
 */

#define SONG_PATH "src/demo/data/song/001-anitra.mid"
//#define SONG_PATH "src/demo/data/song/002-rach2.mid"
//#define SONG_PATH "src/demo/data/song/004-maple.mid"
//#define SONG_PATH "src/demo/data/song/005-enigma.mid"

#define BUFFER_SIZE 512 /* frames */

#undef BB_DEMO_RATE
#define BB_DEMO_RATE 44100

#undef BB_DEMO_DRIVER
#define BB_DEMO_DRIVER 0

#undef BB_DEMO_SYNTH
#define BB_DEMO_SYNTH 'b'

#undef BB_DEMO_CHANC
#define BB_DEMO_CHANC 2

#undef BB_DEMO_REPORT_PERFORMANCE
#define BB_DEMO_REPORT_PERFORMANCE 0

#undef BB_DEMO_MIDI_IN
#define BB_DEMO_MIDI_IN 0

//#undef BB_DEMO_BBB_CACHE_PATH
//#define BB_DEMO_BBB_CACHE_PATH BB_MIDDIR"/bbbcache"

static void demo_bbb_redline_quit() {
}

static int demo_bbb_redline_init() {

  if (!demo_bbb) {
    fprintf(stderr,"bbb not initialized\n");
    return -1;
  }

  void *src=0;
  int srcc=bb_file_read(&src,SONG_PATH);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",SONG_PATH);
    return -1;
  }
  
  struct bb_midi_file *file=bb_midi_file_new(src,srcc);
  free(src);
  if (!file) {
    fprintf(stderr,"%s: Failed to decode MIDI file\n",SONG_PATH);
    return -1;
  }
  
  if (bbb_context_play_song(demo_bbb,file,0)<0) {
    fprintf(stderr,"%s: Failed to begin playback\n",SONG_PATH);
    bb_midi_file_del(file);
    return -1;
  }
  bb_midi_file_del(file);
  
  double starttime_cpu=bb_demo_cpu_now();
  int framec=0;
  int16_t buffer[BUFFER_SIZE*2]; // 2 instead of BB_DEMO_CHANC, because the driver might force 1 or 2
  int samplec=BUFFER_SIZE*bbb_context_get_chanc(demo_bbb);
  int16_t lo=0,hi=0;
  
  while (bbb_context_get_song(demo_bbb)) {
    bbb_context_update(buffer,samplec,demo_bbb);
    framec+=BUFFER_SIZE;
    if (framec>0x10000000) {
      fprintf(stderr,"!!! PANIC !!! Running too long, abort.\n");
      break;
    }
    /**/
    const int16_t *v=buffer;
    int i=samplec;
    for (;i-->0;v++) {
      if (*v<lo) lo=*v;
      else if (*v>hi) hi=*v;
    }
    /**/
  }
  
  /* === How to read this report ===
   * framec,len: How much audio we produced.
   * cpu: Absolute CPU time elapsed.
   * score,x: How much faster than real time? Anything under 10x should be seriously troubling. 100x is good.
   * peak: Highest output sample (mind that we observe this after mixdown; we are not necessarily aware of clipping).
   * memory: If available, how much memory did the process require.
   * evictc: How many times did the store evict PCMs? Really ought to be zero, during a single song.
   *   If evictc greater than zero, consider increasing bbb_store_set_memory_limit() or optimizing instruments (eg reduce velocity mask).
   * pcmc: How many PCMs in cache at the end? If (evictc==0) this is also the total count of PCMs.
   */
  if (framec>0) {
    double produced=(double)framec/(double)bbb_context_get_rate(demo_bbb);
    double time_cpu=bb_demo_cpu_now()-starttime_cpu;
    double score=time_cpu/produced;
    struct rusage rusage={0};
    getrusage(RUSAGE_SELF,&rusage);
    lo=-lo;
    int peak=(lo>hi)?lo:hi;
    int evictionc=bbb_store_get_eviction_count(bbb_context_get_store(demo_bbb));
    int pcmc=bbb_store_get_pcm_count(bbb_context_get_store(demo_bbb));
    fprintf(stderr,
      "%s: rate=%d framec=%d len=%.03fs cpu=%.03fs score=%.06f [%.0fx] peak=%d memory=%ldMB evictc=%d pcmc=%d\n",
      SONG_PATH,bbb_context_get_rate(demo_bbb),framec,produced,
      time_cpu,score,1.0/score,peak,rusage.ru_maxrss>>10,
      evictionc,pcmc
    );
  } else {
    fprintf(stderr,"%s: No signal produced.\n",SONG_PATH);
  }
  
  return 0;
}

static int demo_bbb_redline_update() {
  return 0;
}

BB_DEMO(bbb_redline)

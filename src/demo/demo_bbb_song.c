#include "bb_demo.h"
#include "bbb/bbb.h"
#include "share/bb_fs.h"
#include "share/bb_midi.h"

#define SONG_PATH "src/demo/data/song/001-anitra.mid"
//#define SONG_PATH "src/demo/data/song/002-rach2.mid"
//#define SONG_PATH "src/demo/data/song/004-maple.mid"
//#define SONG_PATH "src/demo/data/song/005-enigma.mid"

#undef BB_DEMO_RATE
#define BB_DEMO_RATE 44100

#undef BB_DEMO_CHANC
#define BB_DEMO_CHANC 2

#undef BB_DEMO_SYNTH
#define BB_DEMO_SYNTH 'b'

static void demo_bbb_song_quit() {
}

static int demo_bbb_song_init() {

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
  
  if (bbb_context_play_song(demo_bbb,file,1)<0) {
    fprintf(stderr,"%s: Failed to begin playback\n",SONG_PATH);
    bb_midi_file_del(file);
    return -1;
  }
  bb_midi_file_del(file);
  
  fprintf(stderr,"%s: Playing\n",SONG_PATH);
  
  return 0;
}

static int demo_bbb_song_update() {
  return 1;
}

BB_DEMO(bbb_song)

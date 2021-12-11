#include "bb_demo.h"
#include "bba/bba.h"
#include "share/bb_fs.h"

static const uint8_t bba_song_bba_song[]={
  0xcc,0x02, // shape
  0x20,0x90,0x88,
  0x20,0x92,0x98,
  0x20,0x93,0xa8,
  0x20,0x95,0xb8,
  0x20,0x97,0xc8,
  0x20,0x98,0xd8,
  0x20,0x9a,0xe8,
  0x20,0x9c,0xf8,
  0x20,0x97,0x40,
  0x40,0x97,0x00,
};

static void *bba_song_song=0;
static int bba_song_songc=0;

static void demo_bba_song_quit() {
  if (bba_song_song) free(bba_song_song);
  bba_song_song=0;
  bba_song_songc=0;
}

static int demo_bba_song_init() {

  if (demo_bba.mainrate) {
    fprintf(stderr,"Playing song through bba...\n");
    const char *songpath="src/demo/data/song-001-anitra.bba";
    //const char *songpath="src/demo/data/song-002-rach2.bba";
    //const char *songpath="src/demo/data/song-003-crow-no-mo.bba";
    if ((bba_song_songc=bb_file_read(&bba_song_song,songpath))>=0) {
      bba_synth_play_song(&demo_bba,bba_song_song,bba_song_songc);
    } else {
      bba_synth_play_song(&demo_bba,bba_song_bba_song,sizeof(bba_song_bba_song));
    }
  }
  
  return 0;
}

static int demo_bba_song_update() {
  return 1;
}

BB_DEMO(bba_song)

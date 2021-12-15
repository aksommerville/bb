#include "bb_demo.h"
#include "bba/bba.h"
#include "share/bb_fs.h"

//#define SONG_PATH 0 /* use hard-coded demo song */
//#define SONG_PATH BB_MIDDIR"/demo/data/song/001-anitra.bba"
//#define SONG_PATH BB_MIDDIR"/demo/data/song/002-rach2.bba"
//#define SONG_PATH BB_MIDDIR"/demo/data/song/004-maple.bba"
#define SONG_PATH BB_MIDDIR"/demo/data/song/005-enigma.bba"

static const uint8_t song_hardcoded[]={
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

static void *song=0;
static int songc=0;

static void demo_bba_song_quit() {
  if (song) free(song);
  song=0;
  songc=0;
}

static int demo_bba_song_init() {

  if (!demo_bba.mainrate) {
    fprintf(stderr,"bba not initialized\n");
    return -1;
  }
  
  if (SONG_PATH) {
    if ((songc=bb_file_read(&song,SONG_PATH))>=0) {
      fprintf(stderr,"Playing song %s\n",SONG_PATH);
      bba_synth_play_song(&demo_bba,song,songc);
    } else {
      fprintf(stderr,"%s:WARNING: Failed to read song file. Playing default instead.\n",SONG_PATH);
      bba_synth_play_song(&demo_bba,song_hardcoded,sizeof(song_hardcoded));
    }
  } else {
    fprintf(stderr,"Playing hard-coded demo song.\n");
    bba_synth_play_song(&demo_bba,song_hardcoded,sizeof(song_hardcoded));
  }
  
  return 0;
}

static int demo_bba_song_update() {
  return 1;
}

BB_DEMO(bba_song)

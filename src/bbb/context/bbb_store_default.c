#include "bbb_context_internal.h"

/* Hard-coded default programs.
 * Same basic idea as the Archive format: One-byte pid followed by self-terminated program.
 * No header, and no requirement that they be in order.
 * Um. Do keep them in order though, please.
 */
 
static const uint8_t bbb_default_programs[]={

  0x00, // Acoustic Grand Piano
    0x05, // type=fm1
    0xfa, // master=31, velocity=2
    0x01, // rate
    0x10, // range
    0xe0,0x38,0x28, // level

  0x08, // Celesta
    0x08, // type=fmv
    0xfa, // master=31, velocity=2
    0x60, // rate
    0x80, // range
    0xc0,0x28,0x2c, // level
    0x50,0xff,0x40, // range
      0x40,0x80,
      0x80,0xc0, 0x80,0xff,
      0x00,
      
  0x10, // Drawbar Organ
    0x04, // type=harm1
    0x82, // master=16, velocity=2
    0x07, // 8 coefficients
      0x40,0x0c,0x20,0x04,0x18,0x02,0x10,0x01,
    0xe0,0x8f,0x84,
    
  0x18, // Nylon Guitar
    0x08, // type=fmv
    0x8c, // master=16, velocity=4
    0x10, // rate
    0x10, // range
    0xc0,0x1c,0x68, // level
    0x50,0xff,0x40, // range
      0xf0,0x20,
      0x04,0xc0, 0x02,0xff,
      0x80,0x80, 0x80,0x40,
      0x00,
      
  0x20, // Acoustic Bass
    0x08, // type=fmv
    0x8c, // master=16, velocity=4
    0x08, // rate
    0x18, // range
    0xc0,0x3c,0x68, // level
    0x50,0xff,0x40, // range
      0xf0,0x20,
      0x04,0xc0, 0x02,0xff,
      0x80,0x80, 0x80,0x40,
      0x00,

  0x28, // Violin
    0x03, // type=shape1
    0x82, // master=16, velocity=2
    0x05, // losaw
    0xe0,0xa8,0x68, // level

  0x30, // String Ensemble 1
    0x04, // type=harm1
    0xfa, // master=31, velocity=2
    0x0f, // 16 harmonics,
      0x40,0x10,0x20,0x28,0x10,0x08,0x07,0x06,0x05,0x04,0x04,0x05,0x04,0x03,0x02,0x01,
    0xe0,0xc8,0x68, // level
    
  0x38, // Trumpet
    0x05, // type=fm1
    0xfa, // master=31, velocity=2
    0x03, // rate
    0x40, // range
    0xe0,0xa8,0x64, // level
};

/* Load default programs, main entry point.
 */
 
int bbb_store_load_default(struct bbb_store *store) {
  if (!store) return -1;
  const uint8_t *src=bbb_default_programs;
  int srcc=sizeof(bbb_default_programs);
  int srcp=0;
  while (srcp<srcc) {
    uint8_t pid=src[srcp++];
    const uint8_t *pgm=src+srcp;
    int len=bbb_measure_program(src+srcp,srcc-srcp);
    if (len<0) {
      fprintf(stderr,"!!! Invalid program in hard-coded defaults around %d/%d\n",srcp,srcc);
      return -1;
    }
    srcp+=len;
    if (!store->programv[pid]) {
      if (bbb_store_set_program(store,pid,pgm,len)<0) return -1;
    }
  }
  return 0;
}

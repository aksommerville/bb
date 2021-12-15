#include "bbb_context_internal.h"

/* Hard-coded default programs.
 * Same basic idea as the Archive format: One-byte pid followed by self-terminated program.
 * No header, and no requirement that they be in order.
 * Um. Do keep them in order though, please.
 */
 
static const uint8_t bbb_default_programs[]={
//TODO Write the full default set...
  0x28, // Violin
    0x07, // type=harmv
    0xfa, // master=31, velocity=2
    0x05, // 6 coefficients, unnormalized
      0x80,0x40,0x20,0x10,0x00,0x00,
      0x10,0x18,0x30,0x60,0x10,0x08,
    0xc0,0x38,0x4c, // level
    0x00,0xff,0x30, // mix
      0x10,0xff,
      0x10,0x40,
      0xf0,0x00,
      0x00,
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

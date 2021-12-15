#ifndef BBB_SYNTH_INTERNAL_H
#define BBB_SYNTH_INTERNAL_H

#include "bbb/bbb.h"
#include "share/bb_codec.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
 
struct bbb_program_type {
  uint8_t ptid;
  const char *name;
  int program_objlen;
  int printer_objlen;
  
  /* program_init() must consume exactly the length that bbb_measure_program() reported.
   * Its decoder begins at the payload, the leading type byte is not included.
   */
  void (*program_del)(struct bbb_program *program);
  int (*program_init)(struct bbb_program *program,struct bb_decoder *src);
  uint32_t (*program_pack_sndid)(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity);
  
  /* printer_init() must create (printer->pcm).
   */
  void (*printer_del)(struct bbb_printer *printer);
  int (*printer_init)(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity);
  int (*printer_update)(int16_t *v,int c,struct bbb_printer *printer);
};
 
struct bbb_program {
  const struct bbb_program_type *type;
  struct bbb_context *context; // WEAK
  int refc;
};

// struct bbb_printer defined in the public header.

const struct bbb_program_type *bbb_program_type_by_id(uint8_t ptid);

#endif

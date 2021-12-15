#include "bbb_synth_internal.h"

/* Cleanup.
 */
 
void bbb_program_del(struct bbb_program *program) {
  if (!program) return;
  if (program->refc-->1) return;
  if (program->type->program_del) program->type->program_del(program);
  free(program);
}

/* Retain.
 */
 
int bbb_program_ref(struct bbb_program *program) {
  if (!program) return -1;
  if (program->refc<1) return -1;
  if (program->refc==INT_MAX) return -1;
  program->refc++;
  return 0;
}

/* New.
 */

struct bbb_program *bbb_program_new(struct bbb_context *context,struct bb_decoder *src) {
  if (!context||!src) return 0;
  int ptid=bb_decode_u8(src);
  if (ptid<0) return 0;
  const struct bbb_program_type *type=bbb_program_type_by_id(ptid);
  if (!type) {
    fprintf(stderr,"Unknown BBB program type 0x%02x.\n",ptid);
    return 0;
  }
  
  struct bbb_program *program=calloc(1,type->program_objlen);
  if (!program) return 0;
  
  program->type=type;
  program->context=context;
  program->refc=1;

  if (type->program_init&&(type->program_init(program,src)<0)) {
    bbb_program_del(program);
    return 0;
  }
  
  return program;
}

/* Pack sndid.
 */
 
uint32_t bbb_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!program||!program->type->program_pack_sndid) return (pid<<16)|(noteid<<8)|velocity;
  return program->type->program_pack_sndid(program,pid,noteid,velocity);
}

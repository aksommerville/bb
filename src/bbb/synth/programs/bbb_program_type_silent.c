#include "bbb/synth/bbb_synth_internal.h"

/* Object definitions.
 */
 
struct bbb_program_silent {
  struct bbb_program hdr;
};

struct bbb_printer_silent {
  struct bbb_printer hdr;
};

#define PROGRAM ((struct bbb_program_silent*)program)
#define PRINTER ((struct bbb_printer_silent*)printer)

/* Cleanup.
 */
 
static void _silent_program_del(struct bbb_program *program) {
}

static void _silent_printer_del(struct bbb_printer *printer) {
}

/* Init program.
 */
 
static int _silent_program_init(struct bbb_program *program,struct bb_decoder *src) {
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _silent_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  return 0;
}

/* Init printer.
 */
 
static int _silent_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {
  int framec=1;
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  return 0;
}

/* Update printer.
 */
 
static int _silent_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_silent={
  .ptid=BBB_PROGRAM_TYPE_silent,
  .name="silent",
  .program_objlen=sizeof(struct bbb_program_silent),
  .printer_objlen=sizeof(struct bbb_printer_silent),
  .program_del=_silent_program_del,
  .program_init=_silent_program_init,
  .program_pack_sndid=_silent_program_pack_sndid,
  .printer_del=_silent_printer_del,
  .printer_init=_silent_printer_init,
  .printer_update=_silent_printer_update,
};

#include "bbb_synth_internal.h"

/* Delete.
 */
 
void bbb_printer_del(struct bbb_printer *printer) {
  if (!printer) return;
  if (printer->refc-->1) return;
  
  if (printer->type->printer_del) printer->type->printer_del(printer);
  bbb_pcm_del(printer->pcm);
  bbb_program_del(printer->program);
  
  free(printer);
}

/* Retain.
 */
 
int bbb_printer_ref(struct bbb_printer *printer) {
  if (!printer) return -1;
  if (printer->refc<1) return -1;
  if (printer->refc==INT_MAX) return -1;
  printer->refc++;
  return 0;
}

/* New.
 */

struct bbb_printer *bbb_print(struct bbb_program *program,uint8_t noteid,uint8_t velocity) {
  if (!program) return 0;
  if (!program->type->printer_init) return 0;
  
  struct bbb_printer *printer=calloc(1,program->type->printer_objlen);
  if (!printer) return 0;
  
  printer->type=program->type;
  printer->context=program->context;
  printer->refc=1;
  
  if (bbb_program_ref(program)<0) {
    free(printer);
    return 0;
  }
  printer->program=program;
  
  if (printer->type->printer_init(printer,noteid,velocity)<0) {
    bbb_printer_del(printer);
    return 0;
  }
  if (!printer->pcm) {
    fprintf(stderr,
      "Program type '%s', note %02x:%02x, did not produce a pcm container.\n",
      printer->type->name,noteid,velocity
    );
    bbb_printer_del(printer);
    return 0;
  }
  printer->pcm->inprogress=1;
  
  return printer;
}

/* Update.
 */

int bbb_printer_update(struct bbb_printer *printer,int c) {
  if (!printer||!printer->type->printer_update) return 0;
  int remaining=printer->pcm->c-printer->p;
  if (c>remaining) c=remaining;
  if (c<1) {
    printer->pcm->inprogress=0;
    return 0;
  }
  if (printer->type->printer_update(printer->pcm->v+printer->p,c,printer)<0) return -1;
  printer->p+=c;
  return 1;
}

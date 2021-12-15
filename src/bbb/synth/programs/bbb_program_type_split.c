#include "bbb/synth/bbb_synth_internal.h"

/* Object definitions.
 */
 
struct bbb_program_split {
  struct bbb_program hdr;
  
  // I won't bother sorting the list, because overlap is allowed.
  struct bbb_split_range {
    uint8_t notec;
    uint8_t srcnoteid;
    uint8_t dstnoteid;
    struct bbb_program *program;
  } *rangev;
  int rangec,rangea;
  
  // Big-endian: ([0]&0x80) is noteid 0, ([31]&0x01) is noteid 255.
  uint8_t present[32];
  
  // To avoid searching the whole list for sndid normalization purposes, we cache the effective velocity masks.
  uint8_t velmaskv[256];
};

struct bbb_printer_split {
  struct bbb_printer hdr;
  struct bbb_printer **subv;
  int subc,suba;
};

#define PROGRAM ((struct bbb_program_split*)program)
#define PRINTER ((struct bbb_printer_split*)printer)
#define PPROG ((struct bbb_program_split*)(printer->program))

/* Cleanup.
 */
 
static void bbb_split_range_cleanup(struct bbb_split_range *range) {
  bbb_program_del(range->program);
}
 
static void _split_program_del(struct bbb_program *program) {
  if (PROGRAM->rangev) {
    while (PROGRAM->rangec-->0) {
      bbb_split_range_cleanup(PROGRAM->rangev+PROGRAM->rangec);
    }
    free(PROGRAM->rangev);
  }
}

static void _split_printer_del(struct bbb_printer *printer) {
  if (PRINTER->subv) {
    while (PRINTER->subc-->0) bbb_printer_del(PRINTER->subv[PRINTER->subc]);
    free(PRINTER->subv);
  }
}

/* Add range.
 */
 
static int bbb_split_add_range(
  struct bbb_program *program,
  uint8_t notec,
  uint8_t srcnoteid,
  uint8_t dstnoteid,
  struct bbb_program *sub
) {

  if (PROGRAM->rangec>=PROGRAM->rangea) {
    int na=PROGRAM->rangea+8;
    if (na>INT_MAX/sizeof(struct bbb_split_range)) return -1;
    void *nv=realloc(PROGRAM->rangev,sizeof(struct bbb_split_range)*na);
    if (!nv) return -1;
    PROGRAM->rangev=nv;
    PROGRAM->rangea=na;
  }

  if (srcnoteid>0x100-notec) return -1;
  if (dstnoteid>0x100-notec) return -1;
  if (bbb_program_ref(sub)<0) return -1;

  struct bbb_split_range *range=PROGRAM->rangev+PROGRAM->rangec++;
  range->notec=notec;
  range->srcnoteid=srcnoteid;
  range->dstnoteid=dstnoteid;
  range->program=sub;
  return 0;
}

/* Init program.
 */
 
static int _split_program_init(struct bbb_program *program,struct bb_decoder *src) {

  // Decode ranges.
  while (1) {
    int notec=bb_decode_u8(src);
    if (notec<1) break;
    int srcnoteid=bb_decode_u8(src);
    int dstnoteid=bb_decode_u8(src);
    if ((srcnoteid<0)||(dstnoteid<0)) return -1;
    int len=bbb_measure_program((char*)(src->src)+src->srcp,src->srcc-src->srcp);
    if (len<1) return -1;
    struct bbb_program *sub=bbb_program_new(program->context,src);
    if (!sub) return -1;
    int err=bbb_split_add_range(program,notec,srcnoteid,dstnoteid,sub);
    bbb_program_del(sub);
    if (err<0) return -1;
  }
  
  // Check note existence and velocity masks.
  const struct bbb_split_range *range=PROGRAM->rangev;
  int i=PROGRAM->rangec;
  for (;i-->0;range++) {
    struct bbb_program *sub=range->program;
    uint8_t srcnoteid=range->srcnoteid;
    uint8_t dstnoteid=range->dstnoteid;
    int notei=range->notec;
    if (sub->type->program_pack_sndid) {
      for (;notei-->0;srcnoteid++,dstnoteid++) {
        uint32_t sndid=sub->type->program_pack_sndid(sub,0,dstnoteid,0xff);
        if (!sndid) continue;
        PROGRAM->present[srcnoteid>>3]|=0x80>>(srcnoteid&7);
        PROGRAM->velmaskv[srcnoteid]|=(sndid&0xff);
      }
    } else {
      memset(PROGRAM->velmaskv+srcnoteid,0xff,range->notec);
      for (;notei-->0;srcnoteid++,dstnoteid++) {
        PROGRAM->present[srcnoteid>>3]|=0x80>>(srcnoteid&7);
      }
    }
  }

  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _split_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!(PROGRAM->present[noteid>>3]&(0x80>>(noteid&7)))) return 0;
  velocity&=PROGRAM->velmaskv[noteid];
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Add sub-printer to a printer.
 * Returns the sub-printer's frame count.
 */
 
static int bbb_split_add_sub(
  struct bbb_printer *printer,
  struct bbb_program *subprogram,
  uint8_t dstnoteid,
  uint8_t velocity
) {
  if (PRINTER->subc>=PRINTER->suba) {
    int na=PRINTER->suba+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(PRINTER->subv,sizeof(void*)*na);
    if (!nv) return -1;
    PRINTER->subv=nv;
    PRINTER->suba=na;
  }
  struct bbb_printer *sub=bbb_print(subprogram,dstnoteid,velocity);
  if (!sub) return -1;
  PRINTER->subv[PRINTER->subc++]=sub;
  return sub->pcm->c;
}

/* Init printer.
 */
 
static int _split_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  if (!(PPROG->present[noteid>>3]&(0x80>>(noteid&7)))) {
    if (!(printer->pcm=bbb_pcm_new(1))) return -1;
    return 0;
  }
  
  int framec=1;
  struct bbb_split_range *range=PPROG->rangev;
  int i=PPROG->rangec;
  for (;i-->0;range++) {
    if (range->srcnoteid>noteid) continue;
    if (range->srcnoteid+range->notec<=noteid) continue;
    int subframec=bbb_split_add_sub(printer,range->program,noteid-range->srcnoteid+range->dstnoteid,velocity);
    if (subframec<0) return -1;
    if (subframec>framec) framec=subframec;
  }
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static void bbb_split_add(int16_t *dst,int c,const struct bbb_pcm *pcm,int p) {
  if (p>pcm->c-c) c=pcm->c-p;
  const int16_t *src=pcm->v;
  for (;c-->0;dst++,src++) (*dst)+=*src;
}
 
static int _split_printer_update(int16_t *v,int c,struct bbb_printer *printer) {

  // Update sub-printers and add them up into the output.
  // We recklessly assume that (printer->p) is the start position for all printers (should always be so).
  // It's OK to over-update a printer; the wrapper takes care of it.
  int i=PRINTER->subc;
  struct bbb_printer **sub=PRINTER->subv;
  for (;i-->0;sub++) {
    if (bbb_printer_update(*sub,c)<0) return -1;
    bbb_split_add(v,c,(*sub)->pcm,printer->p);
  }

  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_split={
  .ptid=BBB_PROGRAM_TYPE_split,
  .name="split",
  .program_objlen=sizeof(struct bbb_program_split),
  .printer_objlen=sizeof(struct bbb_printer_split),
  .program_del=_split_program_del,
  .program_init=_split_program_init,
  .program_pack_sndid=_split_program_pack_sndid,
  .printer_del=_split_printer_del,
  .printer_init=_split_printer_init,
  .printer_update=_split_printer_update,
};

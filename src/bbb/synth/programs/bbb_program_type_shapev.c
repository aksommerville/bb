#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_shapev {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  uint8_t shape;
  struct bbb_wave *wave;
  struct bbb_env levelenv;
  struct bbb_env pitchenv;
};

struct bbb_printer_shapev {
  struct bbb_printer hdr;
  uint8_t shape;
  uint16_t p;
  uint16_t dp;
  struct bbb_wave *wave;
  uint32_t wavep;
  uint32_t wavedp;
  struct bbb_env levelenv;
  struct bbb_env pitchenv;
};

#define PROGRAM ((struct bbb_program_shapev*)program)
#define PRINTER ((struct bbb_printer_shapev*)printer)
#define PPROG ((struct bbb_program_shapev*)(printer->program))

/* Cleanup.
 */
 
static void _shapev_program_del(struct bbb_program *program) {
  bbb_wave_del(PROGRAM->wave);
}

static void _shapev_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->wave);
}

/* Init program.
 */
 
static int _shapev_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  int shape=bb_decode_u8(src);
  if (shape<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  PROGRAM->shape=shape;
  
  switch (shape) {
    case BBB_SHAPE_SINE: if (!(PROGRAM->wave=bbb_wave_get_sine(program->context))) return -1; break;
    case BBB_SHAPE_LOSQUARE: if (!(PROGRAM->wave=bbb_wave_get_losquare(program->context))) return -1; break;
    case BBB_SHAPE_LOSAW: if (!(PROGRAM->wave=bbb_wave_get_losaw(program->context))) return -1; break;
  }
  if (PROGRAM->wave) {
    if (bbb_wave_ref(PROGRAM->wave)<0) {
      PROGRAM->wave=0;
      return -1;
    }
  }
  
  if (bbb_env_decode(&PROGRAM->levelenv,src,program->context->rate)<0) return -1;
  if (bbb_env_decode(&PROGRAM->pitchenv,src,program->context->rate)<0) return -1;
  bbb_env_forbid_sustain(&PROGRAM->levelenv);
  bbb_env_forbid_sustain(&PROGRAM->pitchenv);
  bbb_env_attenuate(&PROGRAM->levelenv,PROGRAM->master);
  
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _shapev_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {

  if (!PROGRAM->master) return 0;
  velocity&=PROGRAM->velmask;
  
  // Unknown shapes become silence, try to cut them off early.
  switch (PROGRAM->shape) {
    case BBB_SHAPE_SINE:
    case BBB_SHAPE_SQUARE:
    case BBB_SHAPE_SAW:
    case BBB_SHAPE_TRIANGLE:
    case BBB_SHAPE_LOSQUARE:
    case BBB_SHAPE_LOSAW:
    case BBB_SHAPE_NOISE:
      break;
    default: return 0;
  }
  
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Init printer.
 */
 
static int _shapev_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  PRINTER->shape=PPROG->shape;
  if (PPROG->wave) {
    if (bbb_wave_ref(PPROG->wave)<0) return -1;
    PRINTER->wave=PPROG->wave;
    PRINTER->wavedp=(bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;
    PRINTER->dp=PRINTER->wavedp>>16; // we need this for some measurements below
  } else {
    PRINTER->dp=(bb_hz_from_noteidv[noteid&0x7f]*0x10000)/printer->context->rate;
  }

  memcpy(&PRINTER->levelenv,&PPROG->levelenv,sizeof(struct bbb_env));
  memcpy(&PRINTER->pitchenv,&PPROG->pitchenv,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->levelenv,velocity);
  bbb_env_reset(&PRINTER->pitchenv,velocity);

  int framec=bbb_env_calculate_duration(&PRINTER->levelenv);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static int _shapev_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  if (PRINTER->wave) {
    const int16_t *src=PRINTER->wave->v;
    for (;c-->0;v++) {
      int16_t padj=bbb_env_next(&PRINTER->pitchenv);
      uint32_t dp=bb_adjust_pitch_u32(PRINTER->wavedp,padj);
      PRINTER->wavep+=dp;
      int16_t level=bbb_env_next(&PRINTER->levelenv);
      *v=(src[PRINTER->wavep>>BBB_WAVE_FRACTION_SIZE_BITS]*level)>>15;
    }
  } else switch (PRINTER->shape) {

    case BBB_SHAPE_SQUARE: {
        for (;c-->0;v++) {
          int16_t padj=bbb_env_next(&PRINTER->pitchenv);
          uint16_t dp=bb_adjust_pitch_u16(PRINTER->dp,padj);
          PRINTER->p+=dp;
          int16_t level=bbb_env_next(&PRINTER->levelenv);
          if (PRINTER->p&0x8000) *v=-level;
          else *v=level;
        }
      } break;
      
    case BBB_SHAPE_SAW: {
        for (;c-->0;v++) {
          int16_t padj=bbb_env_next(&PRINTER->pitchenv);
          uint16_t dp=bb_adjust_pitch_u16(PRINTER->dp,padj);
          PRINTER->p+=dp;
          int16_t level=bbb_env_next(&PRINTER->levelenv);
          *v=((PRINTER->p*level)>>15);
        }
      } break;
      
    case BBB_SHAPE_TRIANGLE: {
        for (;c-->0;v++) {
          int16_t padj=bbb_env_next(&PRINTER->pitchenv);
          uint16_t dp=bb_adjust_pitch_u16(PRINTER->dp,padj);
          PRINTER->p+=dp;
          int16_t level=bbb_env_next(&PRINTER->levelenv);
          if (PRINTER->p&0x8000) {
            *v=-(int16_t)(((PRINTER->p&0x7fff)*level)>>14);
          } else {
            *v=((PRINTER->p*level)>>14);
          }
        }
      } break;
    
    case BBB_SHAPE_NOISE: {
        for (;c-->0;v++) *v=((rand()&0xffff)*bbb_env_next(&PRINTER->levelenv))>>15;
      } break;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_shapev={
  .ptid=BBB_PROGRAM_TYPE_shapev,
  .name="shapev",
  .program_objlen=sizeof(struct bbb_program_shapev),
  .printer_objlen=sizeof(struct bbb_printer_shapev),
  .program_del=_shapev_program_del,
  .program_init=_shapev_program_init,
  .program_pack_sndid=_shapev_program_pack_sndid,
  .printer_del=_shapev_printer_del,
  .printer_init=_shapev_printer_init,
  .printer_update=_shapev_printer_update,
};

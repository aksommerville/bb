#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_shape1 {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  uint8_t shape;
  struct bbb_wave *wave;
  struct bbb_env env;
};

struct bbb_printer_shape1 {
  struct bbb_printer hdr;
  uint8_t shape;
  uint16_t p;
  uint16_t dp;
  struct bbb_wave *wave;
  uint32_t wavep;
  uint32_t wavedp;
  struct bbb_env env;
};

#define PROGRAM ((struct bbb_program_shape1*)program)
#define PRINTER ((struct bbb_printer_shape1*)printer)
#define PPROG ((struct bbb_program_shape1*)(printer->program))

/* Cleanup.
 */
 
static void _shape1_program_del(struct bbb_program *program) {
  bbb_wave_del(PROGRAM->wave);
}

static void _shape1_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->wave);
}

/* Init program.
 */
 
static int _shape1_program_init(struct bbb_program *program,struct bb_decoder *src) {

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
  
  if (bbb_env_decode(&PROGRAM->env,src,program->context->rate)<0) return -1;
  bbb_env_attenuate(&PROGRAM->env,PROGRAM->master);
  
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _shape1_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {

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
 
static int _shape1_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  PRINTER->shape=PPROG->shape;
  if (PPROG->wave) {
    if (bbb_wave_ref(PPROG->wave)<0) return -1;
    PRINTER->wave=PPROG->wave;
    PRINTER->wavedp=(bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;
    PRINTER->dp=PRINTER->wavedp>>16; // we need this for some measurements below
  } else {
    PRINTER->dp=(bb_hz_from_noteidv[noteid&0x7f]*0x10000)/printer->context->rate;
  }

  memcpy(&PRINTER->env,&PPROG->env,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->env,velocity);
  
  int susc=0,susp=0;
  if (PRINTER->env.flags&0x20) {
    // If (dp) happens to be an exact factor of 64k (ie a power of 2), sustain can be one period.
    // Otherwise let's do 8, for no particular reason.
    // I'm hoping that is long enough to avoid obvious detuning, but short enough to avoid obvious timing flaws.
    if (!PRINTER->dp) { // oh yeah, also there's this :P
      susc=1;
    } else if (0x10000%PRINTER->dp) {
      susc=0x80000/PRINTER->dp;
    } else {
      susc=0x10000/PRINTER->dp;
    }
    susp=bbb_env_get_sustain_time(&PRINTER->env);
    if (bbb_env_hardcode_sustain(&PRINTER->env,susc)<0) return -1;
  }

  int framec=bbb_env_calculate_duration(&PRINTER->env);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  if (susp>=0) {
    printer->pcm->loopa=susp;
    printer->pcm->loopz=susp+susc;
  }
  
  return 0;
}

/* Update printer.
 */
 
static int _shape1_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  if (PRINTER->wave) {
    const int16_t *src=PRINTER->wave->v;
    for (;c-->0;v++) {
      PRINTER->wavep+=PRINTER->wavedp;
      int16_t level=bbb_env_next(&PRINTER->env);
      *v=(src[PRINTER->wavep>>BBB_WAVE_FRACTION_SIZE_BITS]*level)>>15;
    }
  } else switch (PRINTER->shape) {

    case BBB_SHAPE_SQUARE: {
        for (;c-->0;v++) {
          PRINTER->p+=PRINTER->dp;
          int16_t level=bbb_env_next(&PRINTER->env);
          if (PRINTER->p&0x8000) *v=-level;
          else *v=level;
        }
      } break;
      
    case BBB_SHAPE_SAW: {
        for (;c-->0;v++) {
          PRINTER->p+=PRINTER->dp;
          int16_t level=bbb_env_next(&PRINTER->env);
          *v=((PRINTER->p*level)>>15);
        }
      } break;
      
    case BBB_SHAPE_TRIANGLE: {
        for (;c-->0;v++) {
          PRINTER->p+=PRINTER->dp;
          int16_t level=bbb_env_next(&PRINTER->env);
          if (PRINTER->p&0x8000) {
            *v=-(int16_t)(((PRINTER->p&0x7fff)*level)>>14);
          } else {
            *v=((PRINTER->p*level)>>14);
          }
        }
      } break;
    
    case BBB_SHAPE_NOISE: {
        for (;c-->0;v++) *v=((rand()&0xffff)*bbb_env_next(&PRINTER->env))>>15;
      } break;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_shape1={
  .ptid=BBB_PROGRAM_TYPE_shape1,
  .name="shape1",
  .program_objlen=sizeof(struct bbb_program_shape1),
  .printer_objlen=sizeof(struct bbb_printer_shape1),
  .program_del=_shape1_program_del,
  .program_init=_shape1_program_init,
  .program_pack_sndid=_shape1_program_pack_sndid,
  .printer_del=_shape1_printer_del,
  .printer_init=_shape1_printer_init,
  .printer_update=_shape1_printer_update,
};

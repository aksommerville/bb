#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_fm1 {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  struct bbb_wave *wave;
  struct bbb_env env;
};

struct bbb_printer_fm1 {
  struct bbb_printer hdr;
  struct bbb_wave *wave;
  uint32_t p;
  uint32_t dp;
  struct bbb_env env;
};

#define PROGRAM ((struct bbb_program_fm1*)program)
#define PRINTER ((struct bbb_printer_fm1*)printer)
#define PPROG ((struct bbb_program_fm1*)(printer->program))

/* Cleanup.
 */
 
static void _fm1_program_del(struct bbb_program *program) {
  bbb_wave_del(PROGRAM->wave);
}

static void _fm1_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->wave);
}

/* Init program.
 */
 
static int _fm1_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  int rate=bb_decode_u8(src);
  int range=bb_decode_u8(src); // u4.4
  if (range<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  
  const struct bbb_wave *sine=bbb_wave_get_sine(program->context);
  if (!sine) return -1;
  if (!(PROGRAM->wave=bbb_wave_new())) return -1;
  bbb_wave_generate_fm(PROGRAM->wave->v,sine->v,BBB_WAVE_SIZE,rate,range/16.0);
  
  if (bbb_env_decode(&PROGRAM->env,src,program->context->rate)<0) return -1;
  bbb_env_attenuate(&PROGRAM->env,PROGRAM->master);
  
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _fm1_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!PROGRAM->master) return 0;
  velocity&=PROGRAM->velmask;
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Init printer.
 */
 
static int _fm1_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  if (bbb_wave_ref(PPROG->wave)<0) return -1;
  PRINTER->wave=PPROG->wave;
  PRINTER->dp=(bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;

  memcpy(&PRINTER->env,&PPROG->env,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->env,velocity);
  
  int susc=0,susp=-1;
  if (PRINTER->env.flags&0x20) {
    uint16_t dp=PRINTER->dp>>16;
    // If (dp) happens to be an exact factor of 64k (ie a power of 2), sustain can be one period.
    // Otherwise let's do 8, for no particular reason.
    // I'm hoping that is long enough to avoid obvious detuning, but short enough to avoid obvious timing flaws.
    if (!dp) { // oh yeah, also there's this :P
      susc=1;
    } else if (0x10000%dp) {
      susc=0x80000/dp;
    } else {
      susc=0x10000/dp;
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
 
static int _fm1_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  const int16_t *src=PRINTER->wave->v;
  for (;c-->0;v++) {
    PRINTER->p+=PRINTER->dp;
    int16_t level=bbb_env_next(&PRINTER->env);
    *v=(src[PRINTER->p>>BBB_WAVE_FRACTION_SIZE_BITS]*level)>>15;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_fm1={
  .ptid=BBB_PROGRAM_TYPE_fm1,
  .name="fm1",
  .program_objlen=sizeof(struct bbb_program_fm1),
  .printer_objlen=sizeof(struct bbb_printer_fm1),
  .program_del=_fm1_program_del,
  .program_init=_fm1_program_init,
  .program_pack_sndid=_fm1_program_pack_sndid,
  .printer_del=_fm1_printer_del,
  .printer_init=_fm1_printer_init,
  .printer_update=_fm1_printer_update,
};

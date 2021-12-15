#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_harmv {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  struct bbb_wave *wavea;
  struct bbb_wave *waveb;
  struct bbb_env levelenv;
  struct bbb_env mixenv;
};

struct bbb_printer_harmv {
  struct bbb_printer hdr;
  struct bbb_wave *wavea;
  struct bbb_wave *waveb;
  uint32_t p;
  uint32_t dp;
  struct bbb_env levelenv;
  struct bbb_env mixenv;
};

#define PROGRAM ((struct bbb_program_harmv*)program)
#define PRINTER ((struct bbb_printer_harmv*)printer)
#define PPROG ((struct bbb_program_harmv*)(printer->program))

/* Cleanup.
 */
 
static void _harmv_program_del(struct bbb_program *program) {
  bbb_wave_del(PROGRAM->wavea);
  bbb_wave_del(PROGRAM->waveb);
}

static void _harmv_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->wavea);
  bbb_wave_del(PRINTER->waveb);
}

/* Init program.
 */
 
static int _harmv_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  int flags=bb_decode_u8(src);
  if (flags<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  
  if (flags&0x70) return -1;
  int coefc=(flags&0x0f)+1;
  const uint8_t *coefav=0,*coefbv=0;
  if (bb_decode_raw(&coefav,src,coefc)<0) return -1;
  if (bb_decode_raw(&coefbv,src,coefc)<0) return -1;
  
  const struct bbb_wave *sine=bbb_wave_get_sine(program->context);
  if (!sine) return -1;
  if (!(PROGRAM->wavea=bbb_wave_new())) return -1;
  if (!(PROGRAM->waveb=bbb_wave_new())) return -1;
  bbb_wave_generate_harmonics_u8(PROGRAM->wavea->v,sine->v,BBB_WAVE_SIZE,coefav,coefc,flags&0x80);
  bbb_wave_generate_harmonics_u8(PROGRAM->waveb->v,sine->v,BBB_WAVE_SIZE,coefbv,coefc,flags&0x80);
  
  if (bbb_env_decode(&PROGRAM->levelenv,src,program->context->rate)<0) return -1;
  if (bbb_env_decode(&PROGRAM->mixenv,src,program->context->rate)<0) return -1;
  bbb_env_forbid_sustain(&PROGRAM->levelenv);
  bbb_env_forbid_sustain(&PROGRAM->mixenv);
  bbb_env_attenuate(&PROGRAM->levelenv,PROGRAM->master);
  
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _harmv_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!PROGRAM->master) return 0;
  velocity&=PROGRAM->velmask;
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Init printer.
 */
 
static int _harmv_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  if (bbb_wave_ref(PPROG->wavea)<0) return -1;
  PRINTER->wavea=PPROG->wavea;
  if (bbb_wave_ref(PPROG->waveb)<0) return -1;
  PRINTER->waveb=PPROG->waveb;
  PRINTER->dp=(bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;

  memcpy(&PRINTER->levelenv,&PPROG->levelenv,sizeof(struct bbb_env));
  memcpy(&PRINTER->mixenv,&PPROG->mixenv,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->levelenv,velocity);
  bbb_env_reset(&PRINTER->mixenv,velocity);

  int framec=bbb_env_calculate_duration(&PRINTER->levelenv);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static int _harmv_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  const int16_t *asrc=PRINTER->wavea->v,*bsrc=PRINTER->waveb->v;
  for (;c-->0;v++) {
  
    PRINTER->p+=PRINTER->dp;
    
    int mix=bbb_env_next(&PRINTER->mixenv);
    int16_t a=asrc[PRINTER->p>>BBB_WAVE_FRACTION_SIZE_BITS];
    int16_t b=bsrc[PRINTER->p>>BBB_WAVE_FRACTION_SIZE_BITS];
    int16_t sample;
    if (mix<=0) sample=a;
    else if (mix>=0x7fff) sample=b;
    else sample=(a*(0x7fff-mix)+b*mix)>>15;
    
    int16_t level=bbb_env_next(&PRINTER->levelenv);
    *v=(sample*level)>>15;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_harmv={
  .ptid=BBB_PROGRAM_TYPE_harmv,
  .name="harmv",
  .program_objlen=sizeof(struct bbb_program_harmv),
  .printer_objlen=sizeof(struct bbb_printer_harmv),
  .program_del=_harmv_program_del,
  .program_init=_harmv_program_init,
  .program_pack_sndid=_harmv_program_pack_sndid,
  .printer_del=_harmv_printer_del,
  .printer_init=_harmv_printer_init,
  .printer_update=_harmv_printer_update,
};

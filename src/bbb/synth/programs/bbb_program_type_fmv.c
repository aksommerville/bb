#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_fmv {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  double rate;
  double range;
  struct bbb_wave *wave;
  struct bbb_env levelenv;
  struct bbb_env rangeenv;
};

struct bbb_printer_fmv {
  struct bbb_printer hdr;
  struct bbb_wave *wave;
  double rate;
  double range;
  uint32_t mp,mpd;
  uint32_t cp,cpd;
  struct bbb_env levelenv;
  struct bbb_env rangeenv;
};

#define PROGRAM ((struct bbb_program_fmv*)program)
#define PRINTER ((struct bbb_printer_fmv*)printer)
#define PPROG ((struct bbb_program_fmv*)(printer->program))

/* Cleanup.
 */
 
static void _fmv_program_del(struct bbb_program *program) {
  bbb_wave_del(PROGRAM->wave);
}

static void _fmv_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->wave);
}

/* Init program.
 */
 
static int _fmv_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  if (bb_decode_fixed(&PROGRAM->rate,src,1,4)<0) return -1;
  if (bb_decode_fixed(&PROGRAM->range,src,1,4)<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  
  struct bbb_wave *sine=bbb_wave_get_sine(program->context);
  if (bbb_wave_ref(sine)<0) return -1;
  PROGRAM->wave=sine;
  
  if (bbb_env_decode(&PROGRAM->levelenv,src,program->context->rate)<0) return -1;
  if (bbb_env_decode(&PROGRAM->rangeenv,src,program->context->rate)<0) return -1;
  bbb_env_attenuate(&PROGRAM->levelenv,PROGRAM->master);
  
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _fmv_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!PROGRAM->master) return 0;
  velocity&=PROGRAM->velmask;
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Init printer.
 */
 
static int _fmv_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  if (bbb_wave_ref(PPROG->wave)<0) return -1;
  PRINTER->wave=PPROG->wave;
  
  PRINTER->cpd=(bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;
  PRINTER->mpd=(PPROG->rate*bb_hz_from_noteidv[noteid&0x7f]*4294967296.0)/printer->context->rate;

  memcpy(&PRINTER->levelenv,&PPROG->levelenv,sizeof(struct bbb_env));
  memcpy(&PRINTER->rangeenv,&PPROG->rangeenv,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->levelenv,velocity);
  bbb_env_reset(&PRINTER->rangeenv,velocity);

  int framec=bbb_env_calculate_duration(&PRINTER->levelenv);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static int _fmv_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  const int16_t *src=PRINTER->wave->v;
  for (;c-->0;v++) {
    
    int16_t sample=src[PRINTER->cp>>BBB_WAVE_FRACTION_SIZE_BITS];
    int16_t rangectl=bbb_env_next(&PRINTER->rangeenv);
    double range=(rangectl*PPROG->range)/32768.0;
    int16_t msample=src[PRINTER->mp>>BBB_WAVE_FRACTION_SIZE_BITS];
    double mod=(msample*range)/32768.0;
    PRINTER->mp+=PRINTER->mpd;
    PRINTER->cp+=PRINTER->cpd+PRINTER->cpd*mod;
    
    int16_t level=bbb_env_next(&PRINTER->levelenv);
    *v=(sample*level)>>15;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_fmv={
  .ptid=BBB_PROGRAM_TYPE_fmv,
  .name="fmv",
  .program_objlen=sizeof(struct bbb_program_fmv),
  .printer_objlen=sizeof(struct bbb_printer_fmv),
  .program_del=_fmv_program_del,
  .program_init=_fmv_program_init,
  .program_pack_sndid=_fmv_program_pack_sndid,
  .printer_del=_fmv_printer_del,
  .printer_init=_fmv_printer_init,
  .printer_update=_fmv_printer_update,
};

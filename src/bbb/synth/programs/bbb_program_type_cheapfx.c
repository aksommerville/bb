#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_cheapfx {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  struct bbb_cheapfx_config {
    uint8_t noteid,shape,range;
    uint16_t pitch,fmrate;
    struct bbb_env pitchenv;
    struct bbb_env rangeenv;
    struct bbb_env levelenv;
  } *configv;
  int configc,configa;
};

struct bbb_printer_cheapfx {
  struct bbb_printer hdr;
  struct bbb_wave *car,*mod;
  int fmrelative;
  uint32_t cp,cpd;
  uint32_t mp,mpd;
  double range;
  struct bbb_env pitchenv;
  struct bbb_env rangeenv;
  struct bbb_env levelenv;
};

#define PROGRAM ((struct bbb_program_cheapfx*)program)
#define PRINTER ((struct bbb_printer_cheapfx*)printer)
#define PPROG ((struct bbb_program_cheapfx*)(printer->program))

/* Cleanup.
 */
 
static void _cheapfx_program_del(struct bbb_program *program) {
  if (PROGRAM->configv) free(PROGRAM->configv);
}

static void _cheapfx_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->car);
  bbb_wave_del(PRINTER->mod);
}

/* Add one drum config.
 */
 
static int bbb_cheapfx_add_config(struct bbb_program *program,int noteid,struct bb_decoder *src) {
  if (noteid>0xff) return -1;
  int mainrate=bbb_context_get_rate(program->context);
  
  if (PROGRAM->configc&&(noteid<=PROGRAM->configv[PROGRAM->configc-1].noteid)) return -1;
  
  if (PROGRAM->configc>=PROGRAM->configa) {
    int na=PROGRAM->configa+16;
    if (na>INT_MAX/sizeof(struct bbb_cheapfx_config)) return -1;
    void *nv=realloc(PROGRAM->configv,sizeof(struct bbb_cheapfx_config)*na);
    if (!nv) return -1;
    PROGRAM->configv=nv;
    PROGRAM->configa=na;
  }
  
  struct bbb_cheapfx_config *config=PROGRAM->configv+PROGRAM->configc++;
  config->noteid=noteid;
  int n;
  if (bb_decode_intbe(&n,src,2)<0) return -1; config->pitch=n;
  if (bb_decode_intbe(&n,src,2)<0) return -1; config->fmrate=n;
  if (bb_decode_intbe(&n,src,1)<0) return -1; config->shape=n;
  if (bb_decode_intbe(&n,src,1)<0) return -1; config->range=n;
  if (bbb_env_decode(&config->pitchenv,src,mainrate)<0) return -1;
  if (bbb_env_decode(&config->rangeenv,src,mainrate)<0) return -1;
  if (bbb_env_decode(&config->levelenv,src,mainrate)<0) return -1;
  
  bbb_env_forbid_sustain(&config->levelenv);
  bbb_env_attenuate(&config->levelenv,PROGRAM->master);
  
  return 0;
}

/* Init program.
 */
 
static int _cheapfx_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  int noteid=bb_decode_u8(src);
  if (noteid<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  
  while (1) {
    if (bb_decode_assert(src,"\0\0",2)>=0) break;
    if (bbb_cheapfx_add_config(program,noteid,src)<0) return -1;
    noteid++;
  }
  
  return 0;
}

/* Get config.
 */
 
static struct bbb_cheapfx_config *bbb_cheapfx_get_config(const struct bbb_program *program,uint8_t noteid) {
  if (PROGRAM->configc<1) return 0;
  if (noteid<PROGRAM->configv[0].noteid) return 0;
  if (noteid>PROGRAM->configv[PROGRAM->configc-1].noteid) return 0;
  int lo=0,hi=PROGRAM->configc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (noteid<PROGRAM->configv[ck].noteid) hi=ck;
    else if (noteid>PROGRAM->configv[ck].noteid) lo=ck+1;
    else return PROGRAM->configv+ck;
  }
  return 0;
}

/* Pack sndid.
 */
 
static uint32_t _cheapfx_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!PROGRAM->master) return 0;
  if (!bbb_cheapfx_get_config(program,noteid)) return 0;
  velocity&=PROGRAM->velmask;
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Init printer.
 */
 
static int _cheapfx_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  const struct bbb_cheapfx_config *config=bbb_cheapfx_get_config(printer->program,noteid);
  if (!config) return -1;
  
  if (!(PRINTER->mod=bbb_wave_from_shape(printer->context,BBB_SHAPE_SINE))) return -1;
  if (!(PRINTER->car=bbb_wave_from_shape(printer->context,config->shape))) return -1;
  
  PRINTER->cpd=(config->pitch*4294967296.0)/bbb_context_get_rate(printer->context);
  if (config->fmrate&0x8000) { // absolute
    PRINTER->mpd=((config->fmrate&0x7fff)*4294967296.0)/bbb_context_get_rate(printer->context);
  } else {
    PRINTER->mpd=config->fmrate;
    PRINTER->fmrelative=1;
  }
  
  PRINTER->range=config->range/16.0;
  
  memcpy(&PRINTER->pitchenv,&config->pitchenv,sizeof(struct bbb_env));
  memcpy(&PRINTER->rangeenv,&config->rangeenv,sizeof(struct bbb_env));
  memcpy(&PRINTER->levelenv,&config->levelenv,sizeof(struct bbb_env));
  bbb_env_reset(&PRINTER->pitchenv,velocity);
  bbb_env_reset(&PRINTER->rangeenv,velocity);
  bbb_env_reset(&PRINTER->levelenv,velocity);
  
  int framec=bbb_env_calculate_duration(&PRINTER->levelenv);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static int _cheapfx_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  const int16_t *csrc=PRINTER->car->v;
  const int16_t *msrc=PRINTER->mod->v;
  for (;c-->0;v++) {
  
    int16_t pitchctl=bbb_env_next(&PRINTER->pitchenv);
    uint32_t cpd=((uint64_t)PRINTER->cpd*pitchctl)>>15;
    uint32_t mpd=PRINTER->mpd;
    if (PRINTER->fmrelative) {
      mpd=((uint64_t)cpd*PRINTER->mpd)>>4;
    }
    
    int16_t sample=csrc[PRINTER->cp>>BBB_WAVE_FRACTION_SIZE_BITS];
    int16_t rangectl=bbb_env_next(&PRINTER->rangeenv);
    double range=(rangectl*PRINTER->range)/32768.0;
    int16_t msample=msrc[PRINTER->mp>>BBB_WAVE_FRACTION_SIZE_BITS];
    double mod=(msample*range)/32768.0;
    PRINTER->mp+=mpd;
    PRINTER->cp+=cpd+cpd*mod;
    
    int16_t level=bbb_env_next(&PRINTER->levelenv);
    *v=(sample*level)>>15;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_cheapfx={
  .ptid=BBB_PROGRAM_TYPE_cheapfx,
  .name="cheapfx",
  .program_objlen=sizeof(struct bbb_program_cheapfx),
  .printer_objlen=sizeof(struct bbb_printer_cheapfx),
  .program_del=_cheapfx_program_del,
  .program_init=_cheapfx_program_init,
  .program_pack_sndid=_cheapfx_program_pack_sndid,
  .printer_del=_cheapfx_printer_del,
  .printer_init=_cheapfx_printer_init,
  .printer_update=_cheapfx_printer_update,
};

#include "bbb/synth/bbb_synth_internal.h"
#include "bbb/context/bbb_context_internal.h"
#include "share/bb_pitch.h"

/* Object definitions.
 */
 
struct bbb_program_weedrums {
  struct bbb_program hdr;
  uint8_t master;
  uint8_t velmask;
  struct bbb_weedrums_config {
    uint8_t noteid,cls,tone,level,release;
  } *configv;
  int configc,configa;
};

struct bbb_printer_weedrums {
  struct bbb_printer hdr;
  struct bbb_wave *mod,*car;
  uint32_t cp,cpd,cpdd;
  uint32_t mp,mpd;
  double range,ranged;
  struct bbb_env pitchenv;
  struct bbb_env levelenv;
};

#define PROGRAM ((struct bbb_program_weedrums*)program)
#define PRINTER ((struct bbb_printer_weedrums*)printer)
#define PPROG ((struct bbb_program_weedrums*)(printer->program))

/* Cleanup.
 */
 
static void _weedrums_program_del(struct bbb_program *program) {
  if (PROGRAM->configv) free(PROGRAM->configv);
}

static void _weedrums_printer_del(struct bbb_printer *printer) {
  bbb_wave_del(PRINTER->mod);
  bbb_wave_del(PRINTER->car);
}

/* Add one drum config.
 */
 
static int bbb_weedrums_add_config(struct bbb_program *program,int noteid,uint8_t cls,uint8_t level,uint8_t tone,uint8_t release) {
  if (noteid>0xff) return -1;
  
  if (PROGRAM->configc&&(noteid<=PROGRAM->configv[PROGRAM->configc-1].noteid)) return -1;
  
  if (PROGRAM->configc>=PROGRAM->configa) {
    int na=PROGRAM->configa+16;
    if (na>INT_MAX/sizeof(struct bbb_weedrums_config)) return -1;
    void *nv=realloc(PROGRAM->configv,sizeof(struct bbb_weedrums_config)*na);
    if (!nv) return -1;
    PROGRAM->configv=nv;
    PROGRAM->configa=na;
  }
  
  struct bbb_weedrums_config *config=PROGRAM->configv+PROGRAM->configc++;
  config->noteid=noteid;
  config->cls=cls;
  config->level=level;
  config->tone=tone;
  config->release=release;
  
  return 0;
}

/* Init program.
 */
 
static int _weedrums_program_init(struct bbb_program *program,struct bb_decoder *src) {

  int lead=bb_decode_u8(src);
  int noteid=bb_decode_u8(src);
  if (noteid<0) return -1;
  
  PROGRAM->master=lead&0xf8;
  PROGRAM->master|=PROGRAM->master>>5;
  int velbitc=lead&7;
  PROGRAM->velmask=((1<<velbitc)-1)<<(7-velbitc);
  
  while (bb_decoder_remaining(src)) {
    int cls=bb_decode_u8(src);
    if (!cls) break;
    int level=bb_decode_u8(src);
    int tone=bb_decode_u8(src);
    int release=bb_decode_u8(src);
    if (release<0) return -1;
    if (bbb_weedrums_add_config(program,noteid,cls,level,tone,release)<0) return -1;
    noteid++;
  }
  
  return 0;
}

/* Get config.
 */
 
static struct bbb_weedrums_config *bbb_weedrums_get_config(const struct bbb_program *program,uint8_t noteid) {
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
 
static uint32_t _weedrums_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity) {
  if (!PROGRAM->master) return 0;
  if (!bbb_weedrums_get_config(program,noteid)) return 0;
  velocity&=PROGRAM->velmask;
  return (pid<<16)|(noteid<<8)|velocity;
}

/* Begin tom.
 */
 
static int bbb_weedrums_begin_tom(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((70.0+config->tone*0.80)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=0xffffff00|((config->tone&0x0f)<<4);
  PRINTER->mpd=PRINTER->cpd*0.83;
  PRINTER->range=2.0;
  PRINTER->ranged=0.999;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x20|(config->level>>4); // attack time and level
  envserial[2]=0x40|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;

  return 0;
}

/* Begin kick.
 */
 
static int bbb_weedrums_begin_kick(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((30.0+config->tone*0.20)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=0xffffff00|((config->tone&0x0f)<<4);
  PRINTER->mpd=PRINTER->cpd*0.83;
  PRINTER->range=2.0;
  PRINTER->ranged=0.999;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x10|(config->level>>4); // attack time and level
  envserial[2]=0x30|(config->release>>4); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;
  
  return 0;
}

/* Begin snare.
 */
 
static int bbb_weedrums_begin_snare(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((20.0+config->tone*0.60)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=(velocity<<4)|0xf;
  PRINTER->mpd=PRINTER->cpd*12.0;
  PRINTER->range=33.99;
  PRINTER->ranged=1.0;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x00|(config->level>>4); // attack time and level
  envserial[2]=0x20|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;

  return 0;
}

/* Begin hat.
 */
 
static int bbb_weedrums_begin_hat(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((700.0+config->tone*2.0)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=(velocity<<4)|0xf;
  PRINTER->mpd=PRINTER->cpd*12.0;
  PRINTER->range=33.99;
  PRINTER->ranged=0.999+((velocity>>3)&0xf)*0.0001;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x00|(config->level>>4); // attack time and level
  envserial[2]=0x20|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;

  return 0;
}

/* Begin cymbal.
 */
 
static int bbb_weedrums_begin_cymbal(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((700.0+config->tone*2.0)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=0xffffff00;
  PRINTER->mpd=PRINTER->cpd*17.0;
  PRINTER->range=51.99;
  PRINTER->ranged=1.0001;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x00|(config->level>>4); // attack time and level
  envserial[2]=0x08|((velocity>>1)&0x30)|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;

  return 0;
}

/* Begin chirptom.
 */
 
static int bbb_weedrums_begin_chirptom(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  
  PRINTER->cpd=((50.0+config->tone*0.80)*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->cpdd=velocity<<4;
  PRINTER->mpd=(111.0*4294967296.0)/bbb_context_get_rate(printer->context);
  PRINTER->range=62.99;
  PRINTER->ranged=0.992;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x00|(config->level>>4); // attack time and level
  envserial[2]=0x40|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,bbb_context_get_rate(printer->context))<0) return -1;

  return 0;
}

/* Begin timpani.
 */
 
static int bbb_weedrums_begin_timpani(struct bbb_printer *printer,const struct bbb_weedrums_config *config,uint8_t velocity) {
  int mainrate=bbb_context_get_rate(printer->context);

  if (!(PRINTER->car=bbb_wave_from_shape(printer->context,BBB_SHAPE_LOSQUARE))) return -1;
  
  PRINTER->cpd=((30.0+config->tone*0.80)*4294967296.0)/mainrate;
  PRINTER->cpdd=0;
  PRINTER->mpd=(PRINTER->cpd*0.900*4294967296.0)/mainrate;
  PRINTER->range=2.5;
  PRINTER->ranged=1.0;
  
  uint8_t penv[]={
    0x40,0xff,0x20,
    0x10,0xc0,0x18,0xff,
    0x10,0x20,0x18,0x40,
  };
  struct bb_decoder penvdec={.src=penv,.srcc=sizeof(penv)};
  if (bbb_env_decode(&PRINTER->pitchenv,&penvdec,mainrate)<0) return -1;

  uint8_t envserial[3]={0xc0,0x00,0x00}; // short, velocity
  envserial[1]=0x00|(config->level>>4); // attack time and level
  envserial[2]=0x40|(config->release>>5); // sustain level and release time
  struct bb_decoder decoder={.src=envserial,.srcc=sizeof(envserial)};
  if (bbb_env_decode(&PRINTER->levelenv,&decoder,mainrate)<0) return -1;

  return 0;
}

/* Init printer.
 */
 
static int _weedrums_printer_init(struct bbb_printer *printer,uint8_t noteid,uint8_t velocity) {

  const struct bbb_weedrums_config *config=bbb_weedrums_get_config(printer->program,noteid);
  if (!config) return -1;
  
  PRINTER->pitchenv.level0lo=0x7fff;
  PRINTER->pitchenv.level0hi=0x7fff;
  PRINTER->pitchenv.level=0x7fff;
  
  switch (config->cls) {
    case 0x01: if (bbb_weedrums_begin_tom(printer,config,velocity)<0) return -1; break;
    case 0x02: if (bbb_weedrums_begin_kick(printer,config,velocity)<0) return -1; break;
    case 0x03: if (bbb_weedrums_begin_snare(printer,config,velocity)<0) return -1; break;
    case 0x04: if (bbb_weedrums_begin_hat(printer,config,velocity)<0) return -1; break;
    case 0x05: if (bbb_weedrums_begin_cymbal(printer,config,velocity)<0) return -1; break;
    case 0x06: if (bbb_weedrums_begin_chirptom(printer,config,velocity)<0) return -1; break;
    case 0x07: if (bbb_weedrums_begin_timpani(printer,config,velocity)<0) return -1; break;
    default: return -1;
  }
  
  if (!PRINTER->mod&&!(PRINTER->mod=bbb_wave_from_shape(printer->context,BBB_SHAPE_SINE))) return -1;
  if (!PRINTER->car&&!(PRINTER->car=bbb_wave_from_shape(printer->context,BBB_SHAPE_SINE))) return -1;

  bbb_env_forbid_sustain(&PRINTER->levelenv);
  bbb_env_attenuate(&PRINTER->levelenv,PPROG->master);
  bbb_env_reset(&PRINTER->levelenv,velocity);
  bbb_env_reset(&PRINTER->pitchenv,velocity);
  int framec=bbb_env_calculate_duration(&PRINTER->levelenv);
  if (!(printer->pcm=bbb_pcm_new(framec))) return -1;
  
  return 0;
}

/* Update printer.
 */
 
static int _weedrums_printer_update(int16_t *v,int c,struct bbb_printer *printer) {
  const int16_t *csrc=PRINTER->car->v;
  const int16_t *msrc=PRINTER->mod->v;
  for (;c-->0;v++) {
  
    int16_t pitchctl=bbb_env_next(&PRINTER->pitchenv);
    uint32_t cpd=((uint64_t)PRINTER->cpd*pitchctl)>>15;
    PRINTER->cpd+=PRINTER->cpdd; // cpdd is obviated by pitchenv, but keeping instead of updating the older classes
    
    int16_t sample=csrc[PRINTER->cp>>BBB_WAVE_FRACTION_SIZE_BITS];
    int16_t msample=msrc[PRINTER->mp>>BBB_WAVE_FRACTION_SIZE_BITS];
    double mod=(msample*PRINTER->range)/32768.0;
    PRINTER->mp+=PRINTER->mpd;
    PRINTER->cp+=cpd+cpd*mod;
    PRINTER->range*=PRINTER->ranged;
    
    int16_t level=bbb_env_next(&PRINTER->levelenv);
    *v=(sample*level)>>15;
  }
  return 0;
}

/* Type definition.
 */

const struct bbb_program_type bbb_program_type_weedrums={
  .ptid=BBB_PROGRAM_TYPE_weedrums,
  .name="weedrums",
  .program_objlen=sizeof(struct bbb_program_weedrums),
  .printer_objlen=sizeof(struct bbb_printer_weedrums),
  .program_del=_weedrums_program_del,
  .program_init=_weedrums_program_init,
  .program_pack_sndid=_weedrums_program_pack_sndid,
  .printer_del=_weedrums_printer_del,
  .printer_init=_weedrums_printer_init,
  .printer_update=_weedrums_printer_update,
};

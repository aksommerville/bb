#include "bbb_synth_internal.h"

/* Decode short format.
 */
 
static const uint16_t bbb_env_short_attack_time[16]={
  8,11,14,16,20,25,30,35,40,50,60,70,80,90,100,120,
};

static const uint8_t bbb_env_short_attack_level[16]={
//  0x20,0x28,0x30,0x3c,0x48,0x54,0x60,0x70,0x80,0x90,0x98,0xa0,0xa8,0xb0,0xb8,0xc0,
//  0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x80,
  0x02,0x04,0x06,0x0a,0x0e,0x14,0x1a,0x20,0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60,
};

static const uint16_t bbb_env_short_release_time[16]={
  20,30,40,60,80,100,130,160,200,250,300,400,500,600,700,800,
};
 
static int bbb_env_decode_short(struct bbb_env *env,uint16_t payload,int mainrate) {
  
  // Input parameters are all in 0..15.
  int atkt=payload>>12;
  int atkl=(payload>>8)&15;
  int susl=(payload>>4)&15;
  int rlst=payload&15;
  
  // Set a few universal assumptions.
  env->level0lo=env->level0hi=0;
  env->pointc=3;
  env->sustainp=1;
  struct bbb_env_point *patk=env->pointv+0;
  struct bbb_env_point *psus=env->pointv+1;
  struct bbb_env_point *prls=env->pointv+2;
  
  // Attack time, attack level, and release time come from static lists.
  patk->timelo=(bbb_env_short_attack_time[atkt]*mainrate)/1000;
  patk->levello=bbb_env_short_attack_level[atkl]<<7;
  patk->levello|=patk->levello>>8;
  prls->timelo=(bbb_env_short_release_time[rlst]*mainrate)/1000;
  prls->levello=0;
  
  // Sustain level is a fraction of attack level.
  psus->levello=((susl+1)*patk->levello)>>4;
  
  // Decay time is a bit longer than attack time, by a constant ratio.
  psus->timelo=patk->timelo*2.00;
  
  // If velocity is in play, nudge the points one way or another.
  if (env->flags&0x40) {
    patk->timehi=patk->timelo*  0.75; patk->timelo*= 1.25;
    patk->levelhi=patk->levello*1.10; patk->levello*=0.50;
    psus->timehi=psus->timelo*  0.75; psus->timelo*= 1.25;
    psus->levelhi=psus->levello*1.25; psus->levello*=0.60;
    prls->timehi=prls->timelo*  1.50; prls->timelo*= 0.50;
    
  // No velocity, copy all 'lo' to 'hi' just to be safe.
  } else {
    patk->timehi=patk->timelo;
    patk->levelhi=patk->levello;
    psus->timehi=psus->timelo;
    psus->levelhi=psus->levello;
    prls->timehi=prls->timelo;
    prls->levelhi=prls->levello;
  }
  
  return 0;
}

/* Decode long format.
 */
 
static int bbb_env_decode_long(struct bbb_env *env,struct bb_decoder *decoder,int mainrate) {

  int levelscale=bb_decode_u8(decoder);
  if (levelscale<0) return -1;
  levelscale<<=7;
  levelscale|=levelscale>>8; // 0..0x7fff
  
  int timescale=bb_decode_u8(decoder); // u2.6
  if (timescale<0) return -1;
  timescale=(timescale*mainrate)>>6; // frames
  
  int levelsize=(env->flags&0x08)?2:1;
  int timesize=(env->flags&0x04)?2:1;
  int levelshift=(levelsize==1)?8:16;
  int timeshift=(timesize==1)?8:16;
  int levelsigned=(env->flags&0x02);
  int n; // contains the natural value after RDL or RDT
  #define RDL ({ \
    if (bb_decode_intbe(&n,decoder,levelsigned?-levelsize:levelsize)<0) return -1; \
    ((n*levelscale)>>levelshift); \
  })
  #define RDT ({ \
    if (bb_decode_intbe(&n,decoder,timesize)<0) return -1; \
    ((n*timescale)>>timeshift); \
  })
  
  if (env->flags&0x20) env->sustainp=bb_decode_u8(decoder);
  
  if (env->flags&0x10) {
    env->level0lo=RDL;
    if (env->flags&0x40) {
      env->level0hi=RDL;
    }
  }
  
  while (bb_decoder_remaining(decoder)) {
    int timelo=RDT;
    if (!n) break;
    if (env->pointc>=BBB_ENV_POINT_LIMIT) return -1;
    struct bbb_env_point *point=env->pointv+env->pointc++;
    point->timelo=timelo;
    point->levello=RDL;
    if (env->flags&0x40) {
      point->timehi=RDT;
      point->levelhi=RDL;
    } else {
      point->timehi=point->timelo;
      point->levelhi=point->levello;
    }
  }
  
  #undef RDL
  #undef RDT
  return 0;
}

/* Decode.
 */

int bbb_env_decode(struct bbb_env *env,struct bb_decoder *decoder,int mainrate) {
  if (!env||!decoder||(mainrate<1)) return -1;
  
  int flags=bb_decode_u8(decoder);
  if (flags<0) return -1;
  env->flags=flags;
  
  if (flags&0x80) {
    int nexttwo;
    if (bb_decode_intbe(&nexttwo,decoder,2)!=2) return -1;
    if (bbb_env_decode_short(env,nexttwo,mainrate)<0) return -1;
  } else {
    if (bbb_env_decode_long(env,decoder,mainrate)<0) return -1;
  }
  
  // Validate.
  if (flags&0x20) {
    if (env->sustainp>=env->pointc) return -1;
  }
  struct bbb_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    if (point->timelo<1) point->timelo=1;
    if (point->timehi<1) point->timehi=1;
  }
  
  // If velocity not in play, set the final state to minimums.
  if (!(flags&0x40)) {
    env->level=env->level0lo;
    for (point=env->pointv,i=env->pointc;i-->0;point++) {
      point->time=point->timelo;
      point->level=point->levello;
    }
  }
  
  return 0;
}

/* Constraints.
 */
 
void bbb_env_forbid_sustain(struct bbb_env *env) {
  env->flags&=~0x20;
  env->sustainp=0xff;
}

void bbb_env_forbid_velocity(struct bbb_env *env) {
  if (env->flags&0x40) {
    env->flags&=~0x40;
    env->level0lo=env->level0hi=env->level=(env->level0lo+env->level0hi)>>1;
    struct bbb_env_point *point=env->pointv;
    int i=env->pointc;
    for (;i-->0;point++) {
      point->timelo=point->timehi=point->time=(point->timelo+point->timehi)>>1;
      point->levello=point->levelhi=point->level=(point->levello+point->levelhi)>>1;
    }
  }
}

void bbb_env_force_zeroes(struct bbb_env *env) {
  env->level=env->level0lo=env->level0hi=0;
  if (env->pointc>1) {
    struct bbb_env_point *point=env->pointv+env->pointc;
    point->level=point->levello=point->levelhi=0;
  }
}

/* Apply velocity.
 */
 
static void bbb_env_velocity_min(struct bbb_env *env) {
  env->level=env->level0lo;
  struct bbb_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->level=point->levello;
    point->time=point->timelo;
  }
}
 
static void bbb_env_velocity_max(struct bbb_env *env) {
  env->level=env->level0hi;
  struct bbb_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->level=point->levelhi;
    point->time=point->timehi;
  }
}
 
static void bbb_env_velocity_mix(struct bbb_env *env,uint8_t hiweight) {
  uint8_t loweight=0x7f-hiweight;
  env->level=(env->level0lo*loweight+env->level0hi*hiweight)>>7;
  struct bbb_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->level=(point->levello*loweight+point->levelhi*hiweight)>>7;
    point->time=(point->timelo*loweight+point->timehi*hiweight)>>7;
  }
}

/* Reset.
 */
 
void bbb_env_reset(struct bbb_env *env,uint8_t velocity) {

  // If we are velocity-sensitive, apply the incoming velocity.
  // These set (level) based on (level0).
  if (env->flags&0x40) {
    if (velocity<=0x00) bbb_env_velocity_min(env);
    else if (velocity>=0x7f) bbb_env_velocity_max(env);
    else bbb_env_velocity_mix(env,velocity);
  } else {
    env->level=env->level0lo;
  }

  env->pointp=0;
  if (env->flags&0x20) env->sustain=1;
  else env->sustain=0;
  env->levela=env->level;
  env->legp=0;
  
  // Begin leg zero or enter the terminal hold.
  if (env->pointc) {
    env->levelr=env->pointv[0].level-env->levela;
    env->legc=env->pointv[0].time;
  } else {
    env->levelr=env->level;
    env->legc=0;
  }
}

/* Release.
 */
  
void bbb_env_release(struct bbb_env *env) {
  if (!env->sustain) return;
  env->sustain=0;
  if (!env->legc&&(env->pointp==env->sustainp)) bbb_env_advance(env);
}

/* Advance to next leg.
 */
 
void bbb_env_advance(struct bbb_env *env) {

  // Hold at sustain point?
  if (env->sustain) {
    if (env->pointp==env->sustainp) {
      env->legp=0;
      env->legc=0;
      env->level=env->pointv[env->pointp].level;
      return;
    }
  }

  env->pointp++;

  // Hold at end?
  if (env->pointp>=env->pointc) {
    env->pointp=env->pointc;
    env->legp=0;
    env->legc=0;
    if (env->pointc) env->level=env->pointv[env->pointc-1].level;
    else env->level=0;
    return;
  }
  
  // Normal point-to-point cases.
  const struct bbb_env_point *b=env->pointv+env->pointp;
  const struct bbb_env_point *a=b-1;
  env->level=a->level;
  env->levela=a->level;
  env->levelr=b->level-a->level;
  env->legp=0;
  env->legc=b->time;
}

/* Hard-code sustain.
 */
 
int bbb_env_hardcode_sustain(struct bbb_env *env,int framec) {
  if (!(env->flags&0x20)) return 0;
  env->flags&=~0x20;
  if (framec<1) return 0;
  if (env->pointc>=BBB_ENV_POINT_LIMIT) return -1;
  env->pointc++;
  struct bbb_env_point *spoint=env->pointv+env->sustainp;
  struct bbb_env_point *npoint=spoint+1;
  memmove(npoint+1,npoint,sizeof(struct bbb_env_point)*(env->pointc-env->sustainp-1));
  memcpy(npoint,spoint,sizeof(struct bbb_env_point));
  npoint->time=npoint->timelo=npoint->timehi=framec;
  env->sustainp=0xff;
  return 0;
}

/* Duration.
 */
 
int bbb_env_calculate_duration(struct bbb_env *env) {
  int dur=0,i=env->pointc;
  const struct bbb_env_point *point=env->pointv;
  for (;i-->0;point++) {
    dur+=point->time;
  }
  if (dur<1) return 1;
  return dur;
}

int bbb_env_get_sustain_time(struct bbb_env *env) {
  int dur=0,i=env->pointc;
  if (env->flags&0x20) i=env->sustainp+1;
  const struct bbb_env_point *point=env->pointv;
  for (;i-->0;point++) dur+=point->time;
  return dur;
}

/* Attenuate.
 */
 
void bbb_env_attenuate(struct bbb_env *env,uint8_t master) {
  struct bbb_env_point *point=env->pointv;
  int i=env->pointc;
  if (!master) {
    for (;i-->0;point++) {
      point->level=point->levello=point->levelhi=0;
    }
  } else if (master<0xff) {
    for (;i-->0;point++) {
      point->level=(point->level*master)>>8;
      point->levello=(point->levello*master)>>8;
      point->levelhi=(point->levelhi*master)>>8;
    }
  }
}

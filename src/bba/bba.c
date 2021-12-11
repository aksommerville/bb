#include "bba.h"
#include <string.h>
#include <stdio.h>

/* Initialize synthesizer.
 */
 
int8_t bba_synth_init(struct bba_synth *synth,uint32_t mainrate) {
  if ((mainrate<BBA_RATE_MIN)||(mainrate>BBA_RATE_MAX)) return -1;
  
  memset(synth,0,sizeof(struct bba_synth));
  synth->mainrate=mainrate;
  
  bba_synth_default_channels(synth);
  
  return 0;
}

/* Update song.
 */
 
static inline void bba_song_update(struct bba_synth *synth) {

  if (!synth->song) return;
  if (synth->songdelay>0) {
    synth->songdelay--;
    return;
  }
  while (1) {
  
    // Repeat at end of song. But also flush state and finish the update.
    if (synth->songp>=synth->songc) {
      if (!synth->songrepeat) {
        bba_synth_play_song(synth,0,0);
        return;
      }
      synth->songp=0;
      synth->songchid=0;
      synth->framespertick=synth->mainrate/BBA_TEMPO_DEFAULT;
      bba_synth_default_channels(synth);
      return;
    }
    
    uint8_t lead=synth->song[synth->songp++];
  
    // Delay. A delay of zero does not finish the update; just skip it.
    if (!lead) continue;
    if (!(lead&0x80)) {
      synth->songdelay=(lead&0x7f)*synth->framespertick;
      return;
    }
    
    // Note.
    if (!(lead&0x40)) {
      if (synth->songp>synth->songc-1) { // missing payload, invalid
        bba_synth_play_song(synth,0,0);
        return;
      }
      uint8_t noteid=lead&0x3f;
      uint8_t velocity=synth->song[synth->songp++];
      bba_synth_note(synth,synth->songchid,noteid,velocity);
      continue;
    }
    
    // Channel property.
    if (!(lead&0x20)) {
      if (synth->songp>synth->songc-1) { // missing payload, invalid
        bba_synth_play_song(synth,0,0);
        return;
      }
      uint8_t k=lead&0x1f;
      uint8_t v=synth->song[synth->songp++];
      bba_channel_set_property(synth,synth->channelv+synth->songchid,k,v);
      continue;
    }
    
    // Active channel.
    if (!(lead&0x10)) {
      synth->songchid=lead&0x0f;
      continue;
    }
    
    // All else invalid.
    bba_synth_play_song(synth,0,0);
    return;
  }
}

/* Update voice.
 * This one function is the closest thing we have to looking like a synthesizer. :)
 */
 
static inline int16_t bba_voice_update(struct bba_synth *synth,struct bba_voice *voice) {

  // Get out quick if shape is SILENCE; that's the usual "unused" indicator.
  if (voice->shape==BBA_VOICE_SHAPE_SILENCE) return 0;
  
  // Update envelope.
  // (c==0) means the envelope is in a hold, either sustain or term.
  // Otherwise, if we drop it to zero, we must advance to the next leg.
  if (voice->env.c) {
    voice->env.v+=voice->env.d;
    voice->env.c--;
    if (!voice->env.c) {
      // Advance to next leg or hold.
      switch (voice->env.stage) {
        case BBA_ENV_STAGE_ATTACK: {
            voice->env.stage=BBA_ENV_STAGE_DECAY;
            voice->env.d=voice->env.decd;
            voice->env.c=voice->env.decc;
          } break;
        case BBA_ENV_STAGE_DECAY: {
            if (voice->env.sustain) {
              // Stay where we are until manually released.
            } else {
              voice->env.stage=BBA_ENV_STAGE_RELEASE;
              voice->env.d=voice->env.rlsd;
              voice->env.c=voice->env.rlsc;
            }
          } break;
        case BBA_ENV_STAGE_RELEASE: {
            voice->shape=BBA_VOICE_SHAPE_SILENCE;
            return 0;
          }
      }
    }
  } else if (voice->env.stage==BBA_ENV_STAGE_RELEASE) {
    // Safety check, if we hit (c==0) in RELEASE, we must terminate.
    voice->shape=BBA_VOICE_SHAPE_SILENCE;
    return 0;
  }
  
  // Advance oscillator.
  voice->p+=voice->rate;
  
  // Combine oscillator and envelope.
  switch (voice->shape) {
  
    case BBA_VOICE_SHAPE_SQUARE: {
        int16_t v=voice->env.v>>17;
        if (voice->p&0x8000) v=-v;
        return v;
      }
      
    case BBA_VOICE_SHAPE_SAW: {
        uint32_t range=voice->env.v>>16;
        int16_t v=((voice->p*range)>>16)-(range>>1);
        return v;
      }
  
  }
  // Unknown shape? Who knows. Kill it.
  voice->shape=BBA_VOICE_SHAPE_SILENCE;
  return 0;
}

/* Update PCM runner.
 */
 
static inline int16_t bba_pcm_update(struct bba_synth *synth,struct bba_pcm *pcm) {
  if (pcm->c) {
    pcm->c--;
    return *(pcm->v++);
  }
  return 0;
}

/* Post-process one sample.
 */
 
static inline int32_t bba_post_update(struct bba_synth *synth,int32_t v) {
  v/=BBA_LOWPASS_LENGTH;
  synth->lpavg-=synth->lpv[synth->lpp];
  synth->lpavg+=v;
  synth->lpv[synth->lpp]=v;
  synth->lpp++;
  if (synth->lpp>=BBA_LOWPASS_LENGTH) synth->lpp=0;
  return v;
}

/* Update, main entry point.
 */

int16_t bba_synth_update(struct bba_synth *synth) {
  int32_t sample=0;
  uint8_t i;
  
  bba_song_update(synth);
  
  struct bba_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) sample+=bba_voice_update(synth,voice);
  
  struct bba_pcm *pcm=synth->pcmv;
  for (i=synth->pcmc;i-->0;pcm++) sample+=bba_pcm_update(synth,pcm);
  
  sample=bba_post_update(synth,sample);
  
  if (sample<-32768) return -32768;
  if (sample>32767) return 32767;
  return sample;
}

/* Silence voices and PCMs.
 */

void bba_synth_quiet(struct bba_synth *synth) {
  synth->voicec=0;
  synth->pcmc=0;
}

/* Begin PCM.
 */

void bba_synth_play_pcm(struct bba_synth *synth,const int16_t *v,uint16_t c) {
  if (!v||!c) return;
  
  // Find an available PCM runner, or bump whichever is closest to done.
  while (synth->pcmc&&!synth->pcmv[synth->pcmc-1].c) synth->pcmc--;
  struct bba_pcm *pcm=0;
  if (synth->pcmc<BBA_PCM_LIMIT) {
    pcm=synth->pcmv+synth->pcmc++;
  } else {
    struct bba_pcm *best=synth->pcmv,*q=synth->pcmv;
    uint8_t i=synth->pcmc;
    for (;i-->0;q++) {
      if (!q->c) {
        best=q;
        break;
      } else if (q->c<best->c) {
        best=q;
      }
    }
    pcm=best;
  }
  
  pcm->v=v;
  pcm->c=c;
}

/* Begin song.
 */
 
void bba_synth_play_song(struct bba_synth *synth,const void *v,uint16_t c) {
  if (!v||!c) {
    synth->song=0;
    synth->songp=0;
    synth->songc=0;
    synth->songdelay=0;
    synth->songchid=0;
    synth->songrepeat=0;
    synth->framespertick=0;
  } else {
    synth->song=v;
    synth->songp=0;
    synth->songc=c;
    synth->songdelay=0;
    synth->songchid=0;
    synth->songrepeat=1;
    synth->framespertick=synth->mainrate/BBA_TEMPO_DEFAULT;
  }
  bba_synth_default_channels(synth);
}

/* Initialize env runner.
 */
 
void bba_env_runner_init(struct bba_synth *synth,struct bba_env_runner *runner,struct bba_env *config,uint8_t velocity) {

  runner->v=0;
  runner->stage=BBA_ENV_STAGE_ATTACK;
  
  if (velocity&0x80) { // fire-and-forget
    runner->sustain=0;
    velocity&=0x7f;
  } else if (config->flags&BBA_ENV_SUSTAIN) {
    runner->sustain=1;
  } else {
    runner->sustain=0;
  }
  
  #define VERBATIM(edge) \
    runner->c=((config->atkt##edge+1)*synth->mainrate)/1000; \
    runner->decc=((config->dect##edge+1)*synth->mainrate)/1000; \
    runner->rlsc=((config->rlst##edge+1)*4*synth->mainrate)/1000; \
    int32_t atkl=config->atkl##edge<<23; atkl|=atkl>>8; atkl|=atkl>>16; \
    int32_t decl=config->decl##edge<<23; decl|=decl>>8; decl|=decl>>16; \
    runner->d=atkl/runner->c; \
    runner->decd=(decl-atkl)/runner->decc; \
    runner->rlsd=-decl/runner->rlsc;
  
  if (velocity&&(config->flags&BBA_ENV_VELOCITY)) {
    if (velocity==0x7f) {
      VERBATIM(hi)
    } else {
      uint8_t lomix=0x7f-velocity;
      #define MIX(dst,lo,hi) { \
        uint32_t ll=(lo),hh=(hi); \
        dst=(ll*lomix+hh*velocity)>>7; \
      }
      MIX(runner->c,
        ((config->atktlo+1)*synth->mainrate)/1000,
        ((config->atkthi+1)*synth->mainrate)/1000
      )
      MIX(runner->decc,
        ((config->dectlo+1)*synth->mainrate)/1000,
        ((config->decthi+1)*synth->mainrate)/1000
      )
      MIX(runner->rlsc,
        ((config->rlstlo+1)*4*synth->mainrate)/1000,
        ((config->rlsthi+1)*4*synth->mainrate)/1000
      )
      int32_t atkl,decl;
      MIX(atkl,
        (config->atkllo<<15)|(config->atkllo<<7)|(config->atkllo>>1),
        (config->atklhi<<15)|(config->atklhi<<7)|(config->atklhi>>1)
      )
      atkl<<=8;
      MIX(decl,
        (config->decllo<<15)|(config->decllo<<7)|(config->decllo>>1),
        (config->declhi<<15)|(config->declhi<<7)|(config->declhi>>1)
      )
      decl<<=8;
      runner->d=atkl/runner->c;
      runner->decd=(decl-atkl)/runner->decc;
      runner->rlsd=-decl/runner->rlsc;
      #undef MIX
    }
  } else {
    VERBATIM(lo)
  }
  #undef VERBATIM
}

/* Initialize env config.
 */

void bba_env_default(struct bba_env *env,struct bba_synth *synth) {
  env->atktlo=0x20;
  env->atkthi=0x05;
  env->atkllo=0x40;
  env->atklhi=0x80;
  env->dectlo=0x18;
  env->decthi=0x10;
  env->decllo=0x10;
  env->declhi=0x30;
  env->rlstlo=0x10;
  env->rlsthi=0x50;
  env->flags=BBA_ENV_VELOCITY|BBA_ENV_SUSTAIN;
}

/* Rate from MIDI note.
 */
 
static const float bba_note_ratev[128]={
  8.175799,8.661957,9.177024,9.722718,10.300861,10.913382,11.562326,12.249857,
  12.978272,13.750000,14.567618,15.433853,16.351598,17.323914,18.354048,19.445436,
  20.601722,21.826764,23.124651,24.499715,25.956544,27.500000,29.135235,30.867706,
  32.703196,34.647829,36.708096,38.890873,41.203445,43.653529,46.249303,48.999429,
  51.913087,55.000000,58.270470,61.735413,65.406391,69.295658,73.416192,77.781746,
  82.406889,87.307058,92.498606,97.998859,103.826174,110.000000,116.540940,123.470825,
  130.812783,138.591315,146.832384,155.563492,164.813778,174.614116,184.997211,195.997718,
  207.652349,220.000000,233.081881,246.941651,261.625565,277.182631,293.664768,311.126984,
  329.627557,349.228231,369.994423,391.995436,415.304698,440.000000,466.163762,493.883301,
  523.251131,554.365262,587.329536,622.253967,659.255114,698.456463,739.988845,783.990872,
  830.609395,880.000000,932.327523,987.766603,1046.502261,1108.730524,1174.659072,1244.507935,
  1318.510228,1396.912926,1479.977691,1567.981744,1661.218790,1760.000000,1864.655046,1975.533205,
  2093.004522,2217.461048,2349.318143,2489.015870,2637.020455,2793.825851,2959.955382,3135.963488,
  3322.437581,3520.000000,3729.310092,3951.066410,4186.009045,4434.922096,4698.636287,4978.031740,
  5274.040911,5587.651703,5919.910763,6271.926976,6644.875161,7040.000000,7458.620184,7902.132820,
  8372.018090,8869.844191,9397.272573,9956.063479,10548.081821,11175.303406,11839.821527,12543.853951,
};
 
uint16_t bba_synth_norm_rate_for_midi_note(const struct bba_synth *synth,uint8_t noteid) {
  return (bba_note_ratev[noteid&0x7f]*0x10000)/synth->mainrate;
}

/* Begin or end note.
 */
 
void bba_synth_note(struct bba_synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=BBA_CHANNEL_COUNT) return;
  
  // Velocity zero means release note.
  if (!velocity) {
    struct bba_voice *voice=synth->voicev;
    uint8_t i=synth->voicec;
    for (;i-->0;voice++) {
      if (voice->chid!=chid) continue;
      if (voice->noteid!=noteid) continue;
      voice->chid=0xff; // ensure no longer addressable
      if (voice->env.sustain) {
        voice->env.sustain=0;
        if (!voice->env.c&&(voice->env.stage==BBA_ENV_STAGE_DECAY)) {
          voice->env.stage=BBA_ENV_STAGE_RELEASE;
          voice->env.c=voice->env.rlsc;
          voice->env.d=voice->env.rlsd;
        }
      }
      return;
    }
    return;
  }
  
  // Select the channel and no-op if it's silent.
  struct bba_channel *channel=synth->channelv+chid;
  if (channel->shape==BBA_VOICE_SHAPE_SILENCE) return;
  
  // Find an unused voice or bump one.
  while (synth->voicec&&(synth->voicev[synth->voicec-1].shape==BBA_VOICE_SHAPE_SILENCE)) synth->voicec--;
  struct bba_voice *voice;
  if (synth->voicec<BBA_VOICE_LIMIT) {
    voice=synth->voicev+synth->voicec++;
  } else {
    struct bba_voice *best=synth->voicev,*q=synth->voicev;
    uint8_t i=synth->voicec;
    for (;i-->0;q++) {
      if (q->shape==BBA_VOICE_SHAPE_SILENCE) {
        best=q;
        break;
      }
      if (q->env.stage>best->env.stage) best=q;
      else if ((q->env.stage==best->env.stage)&&(q->env.c<best->env.c)) best=q;
    }
    voice=best;
  }
  
  // Begin playing.
  voice->chid=chid;
  voice->noteid=noteid;
  voice->p=0;
  voice->rate=bba_synth_norm_rate_for_midi_note(synth,channel->note0+noteid);
  voice->shape=channel->shape;
  bba_env_runner_init(synth,&voice->env,&channel->env,velocity);
}

/* Default channels.
 */
 
void bba_synth_default_channels(struct bba_synth *synth) {
  struct bba_channel *channel=synth->channelv;
  uint8_t i=BBA_CHANNEL_COUNT;
  for (;i-->0;channel++) {
    bba_env_default(&channel->env,synth);
    channel->shape=BBA_VOICE_SHAPE_SAW;
    channel->note0=0x20;
  }
}

/* Set channel property.
 * See etc/doc/bba-song-format.txt for details.
 */
 
void bba_channel_set_property(struct bba_synth *synth,struct bba_channel *channel,uint8_t k,uint8_t v) {
  switch (k) {
    
    case 0x00: if (v) {
        bba_env_default(&channel->env,synth);
        channel->shape=BBA_VOICE_SHAPE_SQUARE;
        channel->note0=0x20;
      } else {
        memset(channel,0,sizeof(struct bba_channel));
      } break;
      
    case 0x01: if (v) {
        channel->env.flags|=BBA_ENV_SUSTAIN;
      } else {
        channel->env.flags&=~BBA_ENV_SUSTAIN;
      } break;
      
    case 0x02: channel->env.atktlo=v; break;
    case 0x03: channel->env.atkthi=v; break;
    case 0x04: channel->env.atkllo=v; break;
    case 0x05: channel->env.atklhi=v; break;
    case 0x06: channel->env.dectlo=v; break;
    case 0x07: channel->env.decthi=v; break;
    case 0x08: channel->env.decllo=v; break;
    case 0x09: channel->env.declhi=v; break;
    case 0x0a: channel->env.rlstlo=v; break;
    case 0x0b: channel->env.rlsthi=v; break;
    
    case 0x0c: channel->shape=v; break;
    case 0x0d: channel->note0=v; break;
    
  }
}

/* bba.h
 * Beepbot, interface A: Maximum efficiency, minimum quality.
 * "A" for "Arduino".
 * Some constraints and curiosities:
 *  - We never allocate memory.
 *  - No cleanup required, for anything -- you must manage lifecycle of PCMs and songs.
 *  - Compile-time limit on simultaneous voices and PCMs.
 *  - Output is produced one sample at a time, mono only.
 * If you're trying to maximize CPU performance, BBB tends to be a better choice (and much better quality).
 * BBA's thing is memory, and it compromises a lot to achieve its memory goals.
 */
 
#ifndef BBA_H
#define BBA_H

#include <stdint.h>

struct bba_synth;

/* Friendly public interface.
 *********************************************************************/

/* Fails only if (mainrate) is unreasonable.
 */
int8_t bba_synth_init(struct bba_synth *synth,uint32_t mainrate);

/* Advance the global clock by one frame, and generate one sample of output.
 */
int16_t bba_synth_update(struct bba_synth *synth);

/* Stop all voices and pcms cold.
 * If a song is playing, it does proceed.
 */
void bba_synth_quiet(struct bba_synth *synth);

/* Begin playing a verbatim PCM dump, or replace the current song.
 * We borrow your buffer and read from it until playback ends -- you must keep it constant.
 * Note that PCMs are limited to 64k samples, and songs to 64k bytes.
 * Play an empty song to stop the current one.
 * Songs repeat by default -- manually set (synth->songrepeat=0) after calling to play just once.
 */
void bba_synth_play_pcm(struct bba_synth *synth,const int16_t *v,uint16_t c);
void bba_synth_play_song(struct bba_synth *synth,const void *v,uint16_t c);

/* Details.
 **********************************************************************/

// These are tweakable but must stay under 256.
#define BBA_VOICE_LIMIT  8
#define BBA_PCM_LIMIT   16

// No particular reason for these limits, but keep them sensible.
#define BBA_RATE_MIN    200
#define BBA_RATE_MAX 200000

// Channel count must be 16; it's hard-wired in the protocols.
#define BBA_CHANNEL_COUNT 16

#define BBA_LOWPASS_LENGTH 3

#define BBA_VOICE_SHAPE_SILENCE 0
#define BBA_VOICE_SHAPE_SQUARE  1
#define BBA_VOICE_SHAPE_SAW     2

#define BBA_ENV_STAGE_ATTACK  0
#define BBA_ENV_STAGE_DECAY   1
#define BBA_ENV_STAGE_RELEASE 2

#define BBA_ENV_VELOCITY    0x01 /* "hi" and "lo" are different, use for velocity sensitivity */
#define BBA_ENV_SUSTAIN     0x02 /* Permit sustain at end of decay. */

#define BBA_TEMPO_DEFAULT 48 /* ticks/s */

struct bba_env {
  uint8_t flags; // BBA_ENV_*
  uint8_t atktlo,atkthi;
  uint8_t atkllo,atklhi;
  uint8_t dectlo,decthi;
  uint8_t decllo,declhi;
  uint8_t rlstlo,rlsthi;
};

struct bba_env_runner {
  uint32_t v; // Current level.
  int32_t d; // Current level slope, per frame.
  uint16_t c; // Remaining length of current leg, in frames.
  uint8_t stage; // BBA_ENV_STAGE_*
  uint8_t sustain; // Nonzero to stop at end of decay until released.
  int32_t decd,rlsd; // Steps for decay and release stage.
  uint16_t decc,rlsc; // Lengths for decay and release stage.
};

struct bba_synth {

  struct bba_voice {
    uint8_t shape;
    uint16_t p;
    uint16_t rate;
    struct bba_env_runner env;
    uint8_t chid,noteid; // noteid encoded format, not midi
  } voicev[BBA_VOICE_LIMIT];
  uint8_t voicec;
  
  struct bba_pcm {
    uint16_t c;
    const int16_t *v;
  } pcmv[BBA_PCM_LIMIT];
  uint8_t pcmc;
  
  const uint8_t *song;
  uint16_t songc;
  uint16_t songp;
  uint32_t songdelay;
  uint8_t songchid;
  uint8_t songrepeat;
  uint32_t framespertick;
  
  struct bba_channel {
    struct bba_env env;
    uint8_t shape;
    uint8_t note0;
  } channelv[BBA_CHANNEL_COUNT];
  
  int32_t lpv[BBA_LOWPASS_LENGTH];
  int16_t lpp;
  int16_t lpavg;
  
  uint32_t mainrate; // hz
};

/* Reset (runner) using (config) as a template.
 * High bit of (velocity) is the "fire-and-forget" flag, guarantees release is not required.
 */
void bba_env_runner_init(struct bba_synth *synth,struct bba_env_runner *runner,struct bba_env *config,uint8_t velocity);

/* Set (env) to some hard-coded default state.
 * This will be sensible for playback.
 */
void bba_env_default(struct bba_env *env,struct bba_synth *synth);

/* Given a MIDI note ID in 0..0x7f, return a normalized rate in 0..0xffff.
 */
uint16_t bba_synth_norm_rate_for_midi_note(const struct bba_synth *synth,uint8_t noteid);

/* (velocity) zero for note off.
 * (velocity&0x80) for fire-and-forget.
 * (noteid) is in the encoded format, 0..63. (not MIDI)
 */
void bba_synth_note(struct bba_synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity);

void bba_synth_default_channels(struct bba_synth *synth);

void bba_channel_set_property(struct bba_synth *synth,struct bba_channel *channel,uint8_t k,uint8_t v);

/* MIDI integration is in a separate optional file.
 * I expect most end products will not need this.
 */
struct bb_midi_event;
void bba_midi_event(struct bba_synth *synth,const struct bb_midi_event *event);

#endif

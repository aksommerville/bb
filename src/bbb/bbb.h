/* bbb.h
 * High-performance, low-quality synthesizer.
 * At the output end, we only use integers.
 * There is no level control or other per-voice expression.
 * So we're playing little snippets of PCM, normally one snippet per note.
 * Those get generated dynamically, that's where we start to look like a synthesizer.
 * Contrast to BBA:
 *  - B uses way more memory.
 *  - B updates in batches, potentially more efficient.
 *  - B has a wider range of expression, more programmable.
 * Neither A nor B supports stereo meaningfully.
 * We do accept >1-channel contexts, and write the same content to each channel.
 */
 
#ifndef BBB_H
#define BBB_H

#include <stdint.h>

struct bbb_context;
struct bbb_pcm;
struct bb_midi_file;
struct bb_midi_event;

/* Context, high level.
 *************************************************************/
 
void bbb_context_del(struct bbb_context *context);
int bbb_context_ref(struct bbb_context *context);

/* (configpath) is an archive or directory containing instrument definitions.
 * Null is legal for the default instrument set.
 * (cachepath) is a directory under which we may store PCM snippets long-term.
 * If it doesn't exist we may create it, but won't create parent directories.
 * Null is legal for no caching.
 */
struct bbb_context *bbb_context_new(
  int rate,int chanc,
  const char *configpath,
  const char *cachepath
);

int bbb_context_get_rate(const struct bbb_context *context);
int bbb_context_get_chanc(const struct bbb_context *context);

void bbb_context_update(int16_t *v,int c,struct bbb_context *context);

/* The basic unit at the playback end is the voice.
 * Each voice is a PCM dump with optional loop.
 * Adding a voice returns 0 if success and not releasable, >0 "voiceid" if release required later.
 * You'll never get a voiceid if (!sustain).
 */
int bbb_context_voice_on_sndid(struct bbb_context *context,uint32_t sndid,int sustain);
int bbb_context_voice_on(struct bbb_context *context,struct bbb_pcm *pcm,int sustain);
void bbb_context_voice_off(struct bbb_context *context,int voiceid);

/* all_off() to release every currently sustaining voice.
 * silence() to drop every voice cold.
 * Silencing does not end the song, or affect it at all.
 */
void bbb_context_all_off(struct bbb_context *context);
void bbb_context_silence(struct bbb_context *context);

/* Replace the current song.
 * Null file is ok, that means no song.
 * Request is ignored if this song is currently playing (and the playhead doesn't reset or anything).
 * If you want to force playback from the start, play null first.
 */
int bbb_context_play_song(struct bbb_context *context,struct bb_midi_file *file,int repeat);

// Weak reference to the current song, null if none.
struct bb_midi_file *bbb_context_get_song(struct bbb_context *context);

// If interested, you can also feed events to the context as if they came off a song.
int bbb_context_event(struct bbb_context *context,const struct bb_midi_event *event);

/* sndid is a combination of (pid,noteid,velocity).
 * But it is normalized first: We may select a different program and eliminate redundant velocity bits.
 * sndid zero is a special value meaning "definitely silent".
 */
uint32_t bbb_sndid(const struct bbb_context *context,uint8_t pid,uint8_t noteid,uint8_t velocity);

//TODO Tempo tracking?

/* PCM objects for playback.
 * In general, think of PCM objects as immutable.
 * But in practice, the signal might be generated piecemeal the first time it runs.
 *********************************************************/
 
struct bbb_pcm {
  int refc;
  int c;
  int loopa,loopz; // Sustainable if (a<z). (0<=a<z<=c)
  int inprogress; // Nonzero if (v) is being asynchronously printed.
  int16_t v[];
};

void bbb_pcm_del(struct bbb_pcm *pcm);
int bbb_pcm_ref(struct bbb_pcm *pcm);

struct bbb_pcm *bbb_pcm_new(int c);

/* Extra details you probably shouldn't care about.
 *****************************************************************/
 
struct bbb_program_type;
struct bbb_program;
struct bb_decoder;

struct bbb_printer {
  const struct bbb_program_type *type;
  struct bbb_context *context; // WEAK
  struct bbb_program *program; // STRONG
  int refc;
  struct bbb_pcm *pcm;
  int p;
};

void bbb_printer_del(struct bbb_printer *printer);
int bbb_printer_ref(struct bbb_printer *printer);

struct bbb_printer *bbb_print(struct bbb_program *program,uint8_t noteid,uint8_t velocity);

// 0 if complete, >0 if more remaining.
int bbb_printer_update(struct bbb_printer *printer,int c);

int bbb_measure_program(const void *src,int srcc);
int bbb_measure_env(const void *src,int srcc);

#define BBB_PROGRAM_TYPE_dummy     0x00
#define BBB_PROGRAM_TYPE_silent    0x01
#define BBB_PROGRAM_TYPE_split     0x02
#define BBB_PROGRAM_TYPE_shape1    0x03
#define BBB_PROGRAM_TYPE_harm1     0x04
#define BBB_PROGRAM_TYPE_fm1       0x05
#define BBB_PROGRAM_TYPE_shapev    0x06
#define BBB_PROGRAM_TYPE_harmv     0x07
#define BBB_PROGRAM_TYPE_fmv       0x08
#define BBB_PROGRAM_TYPE_weedrums  0x09
#define BBB_PROGRAM_TYPE_cheapfx   0x0a

// This doesn't name dummy, because it doesn't count as a real type.
#define BBB_FOR_EACH_PROGRAM_TYPE \
  _(silent) \
  _(split) \
  _(shape1) \
  _(harm1) \
  _(fm1) \
  _(shapev) \
  _(harmv) \
  _(fmv) \
  _(weedrums) \
  _(cheapfx)

/* A program is the constant configuration from which printers are spawned.
 * These get decoded by the store at load time.
 * First byte of an encoded program names its type.
 * For the rest, see etc/doc/bbb-formats.txt.
 */
void bbb_program_del(struct bbb_program *program);
int bbb_program_ref(struct bbb_program *program);
struct bbb_program *bbb_program_new(struct bbb_context *context,struct bb_decoder *src);
uint32_t bbb_program_pack_sndid(struct bbb_program *program,uint8_t pid,uint8_t noteid,uint8_t velocity);

#define BBB_ENV_POINT_LIMIT 8

/* In a live envelope, levels are normalized to 0..0x7fff, and times are in frames.
 */
struct bbb_env {
  int16_t level0lo,level0hi;
  uint8_t flags;
  uint8_t sustainp;
  uint8_t pointp;
  uint8_t sustain;
  int16_t level;
  int16_t levela,levelr;
  int legp,legc;
  uint8_t pointc;
  struct bbb_env_point {
    int time,timelo,timehi;
    int16_t level,levello,levelhi;
  } pointv[BBB_ENV_POINT_LIMIT];
};

int bbb_env_decode(struct bbb_env *env,struct bb_decoder *decoder,int mainrate);

// If you want to apply these constaints, do it after decode and before reset.
void bbb_env_forbid_sustain(struct bbb_env *env);
void bbb_env_forbid_velocity(struct bbb_env *env);
void bbb_env_force_zeroes(struct bbb_env *env);

// Velocities should be in 0..0x7f; anything higher clamps to 0x7f.
void bbb_env_reset(struct bbb_env *env,uint8_t velocity);
void bbb_env_release(struct bbb_env *env);

void bbb_env_advance(struct bbb_env *env);

static inline int16_t bbb_env_next(struct bbb_env *env) {
  if (env->legp>=env->legc) return env->level;
  env->level=env->levela+(env->legp*env->levelr)/env->legc;
  if (++(env->legp)>=env->legc) bbb_env_advance(env);
  return env->level;
}

/* If this env has sustain, and at least one point short of the limit,
 * insert a new leg for the sustain phase, and then make it look unsustaining.
 */
int bbb_env_hardcode_sustain(struct bbb_env *env,int framec);

// Running time in frames for the current velocity, assuming no sustain.
int bbb_env_calculate_duration(struct bbb_env *env);

// Calculate duration up to the sustain point.
int bbb_env_get_sustain_time(struct bbb_env *env);

/* Adjust all levels down, (master) is u0.8.
 * 0 makes silence, and 0xff is noop.
 */
void bbb_env_attenuate(struct bbb_env *env,uint8_t master);

#define BBB_SHAPE_SINE      0
#define BBB_SHAPE_SQUARE    1
#define BBB_SHAPE_SAW       2
#define BBB_SHAPE_TRIANGLE  3
#define BBB_SHAPE_LOSQUARE  4
#define BBB_SHAPE_LOSAW     5
#define BBB_SHAPE_NOISE     6

#ifndef BBB_WAVE_SIZE_BITS
  #define BBB_WAVE_SIZE_BITS 12
#endif
#define BBB_WAVE_SIZE (1<<BBB_WAVE_SIZE_BITS)
#define BBB_WAVE_FRACTION_SIZE_BITS (32-BBB_WAVE_SIZE_BITS)

struct bbb_wave {
  int refc;
  int16_t v[BBB_WAVE_SIZE];
};

void bbb_wave_del(struct bbb_wave *wave);
int bbb_wave_ref(struct bbb_wave *wave);
struct bbb_wave *bbb_wave_new();

// Weak reference to a few common waves that we build and cache on demand.
struct bbb_wave *bbb_wave_get_sine(struct bbb_context *context);
struct bbb_wave *bbb_wave_get_losquare(struct bbb_context *context);
struct bbb_wave *bbb_wave_get_losaw(struct bbb_context *context);

// Convenience to get a STRONG wave from a shape.
struct bbb_wave *bbb_wave_from_shape(struct bbb_context *context,uint8_t shape);

/* Primitive waves.
 * losquare: (sigma) in 0..1 = (square..sine).
 * losaw: (base>=1), higher to approach a pure saw, 1 for a sine. 150 seems nice.
 * harmonics_u8: Combine up to 16 harmonic sines (or whatever base wave you supply).
 * fm: Single period FM. Modulator rate must be a multiple of the implicit carrier rate (note it is an integer).
 */
void bbb_wave_generate_sine(int16_t *v,int c);
void bbb_wave_generate_losquare(int16_t *v,int c,int16_t level,double sigma);
void bbb_wave_generate_losaw(int16_t *v,int c,double base);
void bbb_wave_generate_harmonics_u8(int16_t *v,const int16_t *ref,int c,const uint8_t *coefv,int coefc,int normalize);
void bbb_wave_generate_fm(int16_t *v,const int16_t *ref,int c,uint8_t rate,double range);
void bbb_wave_generate_square(int16_t *v,int c);
void bbb_wave_generate_saw(int16_t *v,int c,int16_t a,int16_t z);
void bbb_wave_generate_triangle(int16_t *v,int c);

#endif

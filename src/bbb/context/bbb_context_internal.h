#ifndef BBB_CONTEXT_INTERNAL_H
#define BBB_CONTEXT_INTERNAL_H

#include "bbb/bbb.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

struct bbb_store;
struct bbb_voice;
struct bb_midi_file_reader;

// Arbitrary sanity limits.
#define BBB_RATE_MIN    200
#define BBB_RATE_MAX 200000
#define BBB_CHANC_MIN 1
#define BBB_CHANC_MAX 8
#define BBB_DEFAULT_VOICE_LIMIT 128

#define BBB_CHANNEL_COUNT 16 /* Should match MIDI, ie 16 */

/* Context, private API.
 ****************************************************************/

struct bbb_context {
  int refc;
  int rate;
  int chanc;
  int voice_limit;
  
  struct bb_midi_file_reader *song;
  struct bbb_channel {
    uint8_t pid;
  } channelv[BBB_CHANNEL_COUNT];
  
  struct bbb_voice *voicev;
  int voicec,voicea;
  int voiceid_next;
  
  struct bbb_store *store;
  
  struct bbb_printer **printerv;
  int printerc,printera;
};

/* Voice, private API.
 *****************************************************************/
 
struct bbb_voice {
  int voiceid; // 0 if not addressable (ie not sustaining)
  struct bbb_pcm *pcm; // null if not in use
  int p;
  uint8_t chid,noteid; // as specified in a midi event, for context's tracking
};

void bbb_voice_cleanup(struct bbb_voice *voice);

/* Blindly overwrites (voice).
 * Provide a nonzero (voiceid) if you want sustain.
 * We validate against (pcm); if it doesn't support sustain, (voice->voiceid) will be zero.
 */
int bbb_voice_setup(struct bbb_voice *voice,int voiceid,struct bbb_pcm *pcm);

/* Adds to (v), you must clear it initially.
 * If we reach the end, we clear (pcm).
 */
void bbb_voice_update(int16_t *v,int c,struct bbb_voice *voice);

/* Store, private API.
 ****************************************************************/
 
struct bbb_store {
  struct bbb_context *context; // WEAK
  int refc;
  
  char *configpath;
  int configpathc;
  char *cachepath;
  int cachepathc;
  
  struct bbb_program *programv[256];
  
  struct bbb_store_entry {
    uint32_t sndid;
    struct bbb_pcm *pcm;
    uint32_t access;
  } *entryv;
  int entryc,entrya;
  uint32_t access_next;
  
  int printc;
  int evictionc;
  int pcmtotal; // Sum of sample counts of all pcm entries.
  int limit_pcmc,target_pcmc;
  int limit_pcmt,target_pcmt;
  
  // No general wave store, but we do keep some common ones ad-hoc.
  struct bbb_wave *wave_sine;
  struct bbb_wave *wave_losquare;
  struct bbb_wave *wave_losaw;
};

void bbb_store_del(struct bbb_store *store);
int bbb_store_ref(struct bbb_store *store);
struct bbb_store *bbb_store_new(struct bbb_context *context,const char *configpath,const char *cachepath);

/* Error only if decode fails. (fail to read is not an error)
 */
int bbb_store_load(struct bbb_store *store);

int bbb_store_set_program(struct bbb_store *store,uint8_t pid,const void *src,int srcc);

/* Fetch a PCM or generate it.
 * Returns STRONG. The object is probably cached but maybe not.
 * If (printer) provided we populate it with a new (STRONG) printer to generate this pcm.
 * If you don't provide (printer), or we set it null, the pcm is complete.
 */
struct bbb_pcm *bbb_store_get_pcm(
  struct bbb_printer **printer,
  struct bbb_store *store,
  uint32_t sndid
);

int bbb_store_search(const struct bbb_store *store,uint32_t sndid);
int bbb_store_insert(struct bbb_store *store,int p,uint32_t sndid,struct bbb_pcm *pcm);
int bbb_store_replace(struct bbb_store *store,int p,struct bbb_pcm *pcm);

/* Context should call this when a printer finishes.
 * If we are configured with a disk cache, this writes the PCM to it.
 */
int bbb_store_print_finished(struct bbb_store *store,struct bbb_pcm *pcm);

/* Any program that isn't populated, make something up.
 * (may leave programs unset too).
 */
int bbb_store_load_default(struct bbb_store *store);

#endif

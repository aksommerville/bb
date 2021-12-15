#include "bbb_context_internal.h"
#include "share/bb_midi.h"

/* Cleanup.
 */
 
void bbb_context_del(struct bbb_context *context) {
  if (!context) return;
  if (context->refc-->1) return;

  bb_midi_file_reader_del(context->song);
  bbb_store_del(context->store);
  
  if (context->voicev) {
    while (context->voicec-->0) {
      bbb_voice_cleanup(context->voicev+context->voicec);
    }
    free(context->voicev);
  }
  
  if (context->printerv) {
    while (context->printerc-->0) {
      bbb_printer_del(context->printerv[context->printerc]);
    }
    free(context->printerv);
  }
  
  free(context);
}

/* Retain.
 */
 
int bbb_context_ref(struct bbb_context *context) {
  if (!context) return -1;
  if (context->refc<1) return -1;
  if (context->refc==INT_MAX) return -1;
  context->refc++;
  return 0;
}

/* New.
 */

struct bbb_context *bbb_context_new(
  int rate,int chanc,
  const char *configpath,
  const char *cachepath
) {
  if ((rate<BBB_RATE_MIN)||(rate>BBB_RATE_MAX)) return 0;
  if ((chanc<BBB_CHANC_MIN)||(chanc>BBB_CHANC_MAX)) return 0;
  
  struct bbb_context *context=calloc(1,sizeof(struct bbb_context));
  if (!context) return 0;
  
  context->refc=1;
  context->rate=rate;
  context->chanc=chanc;
  context->voiceid_next=1;
  context->voice_limit=BBB_DEFAULT_VOICE_LIMIT;
  
  if (!(context->store=bbb_store_new(context,configpath,cachepath))) {
    bbb_context_del(context);
    return 0;
  }
  if (bbb_store_load(context->store)<0) {
    bbb_context_del(context);
    return 0;
  }
  
  return context;
}

/* Trivial accessors.
 */
 
struct bb_midi_file *bbb_context_get_song(struct bbb_context *context) {
  if (!context||!context->song) return 0;
  return context->song->file;
}

int bbb_context_get_rate(const struct bbb_context *context) {
  if (!context) return 0;
  return context->rate;
}

int bbb_context_get_chanc(const struct bbb_context *context) {
  if (!context) return 0;
  return context->chanc;
}

struct bbb_store *bbb_context_get_store(const struct bbb_context *context) {
  if (!context) return 0;
  return context->store;
}

/* Add printer.
 */
 
static int bbb_context_add_printer(struct bbb_context *context,struct bbb_printer *printer) {
  if (context->printerc>=context->printera) {
    int na=context->printera+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(context->printerv,sizeof(void*)*na);
    if (!nv) return -1;
    context->printerv=nv;
    context->printera=na;
  }
  if (bbb_printer_ref(printer)<0) return -1;
  context->printerv[context->printerc++]=printer;
  return 0;
}

/* Add voice.
 */
 
static struct bbb_voice *bbb_context_add_voice(struct bbb_context *context,int voiceid,struct bbb_pcm *pcm) {
  
  struct bbb_voice *voice=0;
  if (context->voicec<context->voicea) {
    voice=context->voicev+context->voicec++;
  } else {
    struct bbb_voice *q=context->voicev;
    int i=context->voicec;
    for (;i-->0;q++) {
      if (!q->pcm) {
        voice=q;
        break;
      }
    }
    if (!voice) {
      if (context->voicea>=context->voice_limit) return 0;
      int na=context->voicea+8;
      if (na>INT_MAX/sizeof(struct bbb_voice)) return 0;
      void *nv=realloc(context->voicev,sizeof(struct bbb_voice)*na);
      if (!nv) return 0;
      context->voicev=nv;
      context->voicea=na;
      voice=context->voicev+context->voicec++;
    }
  }
  
  if (bbb_voice_setup(voice,voiceid,pcm)<0) return 0;
  return voice;
}

/* Note On.
 */
 
static int bbb_context_note_on(struct bbb_context *context,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  
  uint8_t pid=(chid<BBB_CHANNEL_COUNT)?context->channelv[chid].pid:0;
  uint32_t sndid=bbb_sndid(context,pid,noteid,velocity);
  if (!sndid) return 0;
  
  struct bbb_pcm *pcm=0;
  struct bbb_printer *printer=0;
  if (!(pcm=bbb_store_get_pcm(&printer,context->store,sndid))) return 0;
  
  if (printer) {
    int err=bbb_context_add_printer(context,printer);
    bbb_printer_del(printer);
    if (err<0) {
      bbb_pcm_del(pcm);
      return -1;
    }
  }
  
  int voiceid=context->voiceid_next++;
  struct bbb_voice *voice=bbb_context_add_voice(context,voiceid,pcm);
  bbb_pcm_del(pcm);
  if (!voice) return -1;
  
  voice->chid=chid;
  voice->noteid=noteid;
  
  return voiceid;
}

/* New voice by sndid.
 */

int bbb_context_voice_on_sndid(struct bbb_context *context,uint32_t sndid,int sustain) {
  
  struct bbb_pcm *pcm=0;
  struct bbb_printer *printer=0;
  if (!(pcm=bbb_store_get_pcm(&printer,context->store,sndid))) return 0;
  
  if (printer) {
    int err=bbb_context_add_printer(context,printer);
    bbb_printer_del(printer);
    if (err<0) {
      bbb_pcm_del(pcm);
      return -1;
    }
  }
  
  int voiceid=0;
  if (sustain) voiceid=context->voiceid_next++;
  struct bbb_voice *voice=bbb_context_add_voice(context,voiceid,pcm);
  bbb_pcm_del(pcm);
  bbb_printer_del(printer);
  if (!voice) return -1;
  
  return voiceid;
}

/* New voice with pcm.
 */
 
int bbb_context_voice_on(struct bbb_context *context,struct bbb_pcm *pcm,int sustain) {
  
  int voiceid=0;
  if (sustain) voiceid=context->voiceid_next++;
  struct bbb_voice *voice=bbb_context_add_voice(context,voiceid,pcm);
  if (!voice) return -1;
  
  return voiceid;
}

/* Release voice.
 */
 
void bbb_context_voice_off(struct bbb_context *context,int voiceid) {
  if (voiceid<1) return;
  struct bbb_voice *voice=context->voicev;
  int i=context->voicec;
  for (;i-->0;voice++) {
    if (voice->voiceid==voiceid) {
      bbb_voice_cleanup(voice);
      memset(voice,0,sizeof(struct bbb_voice));
      return;
    }
  }
}

/* Silence.
 */

static void bbb_context_all_song_notes_off(struct bbb_context *context) {
  struct bbb_voice *voice=context->voicev;
  int i=context->voicec;
  for (;i-->0;voice++) {
    if (voice->chid!=0xff) {
      voice->voiceid=0;
    }
  }
}

void bbb_context_all_off(struct bbb_context *context) {
  struct bbb_voice *voice=context->voicev;
  int i=context->voicec;
  for (;i-->0;voice++) {
    voice->voiceid=0;
  }
}

void bbb_context_silence(struct bbb_context *context) {
  struct bbb_voice *voice=context->voicev;
  int i=context->voicec;
  for (;i-->0;voice++) {
    bbb_voice_cleanup(voice);
  }
  context->voicec=0;
}

/* Process event.
 */
 
int bbb_context_event(struct bbb_context *context,const struct bb_midi_event *event) {
  
  //fprintf(stderr,"%s %02x %02x %02x %02x\n",__func__,event->opcode,event->chid,event->a,event->b);
  
  switch (event->opcode) {
  
    case BB_MIDI_OPCODE_NOTE_ON: return bbb_context_note_on(context,event->chid,event->a,event->b);
      
    case BB_MIDI_OPCODE_NOTE_OFF: {
        struct bbb_voice *voice=context->voicev;
        int i=context->voicec;
        for (;i-->0;voice++) {
          if (voice->chid!=event->chid) continue;
          if (voice->noteid!=event->a) continue;
          voice->voiceid=0;
          // Don't return; if there's more than one match we want to release all of them.
        }
      } return 0;
      
    case BB_MIDI_OPCODE_PROGRAM: {
        if (event->chid<BBB_CHANNEL_COUNT) {
          context->channelv[event->chid].pid=event->a;
        }
      } return 0;
      
    case BB_MIDI_OPCODE_SYSTEM_RESET: {
        bbb_context_silence(context);
      } return 0;
    
    case BB_MIDI_OPCODE_CONTROL: switch (event->a) {
        case BB_MIDI_CONTROL_SOUND_OFF: bbb_context_silence(context); return 0;
        case BB_MIDI_CONTROL_NOTES_OFF: bbb_context_all_off(context); return 0;
      } return 0;

  }
  return 0;
}

/* Update printers.
 */
 
static int bbb_context_update_printers(struct bbb_context *context,int framec) {
  int i=context->printerc;
  while (i-->0) {
    struct bbb_printer *printer=context->printerv[i];
    int err=bbb_printer_update(printer,framec);
    if (err<=0) {
      bbb_store_print_finished(context->store,printer->pcm);
      context->printerc--;
      memmove(context->printerv+i,context->printerv+i+1,sizeof(void*)*(context->printerc-i));
      bbb_printer_del(printer);
    }
  }
  return 0;
}

/* Update for mono output -- ideal case.
 */
 
static void bbb_context_update_mono(int16_t *v,int c,struct bbb_context *context) {
  while (c>0) {
  
    // Process any song events.
    int updc;
    if (context->song) {
      while (1) {
        struct bb_midi_event event;
        updc=bb_midi_file_reader_update(&event,context->song);
        if (updc<0) {
          bb_midi_file_reader_del(context->song);
          context->song=0;
          updc=c;
          break;
        }
        if (updc) {
          break;
        }
        bbb_context_event(context,&event);
      }
      if (updc>c) updc=c;
    } else {
      updc=c;
    }
    if (updc<1) return; // oops
    if (context->song) {
      bb_midi_file_reader_advance(context->song,updc);
    }
    
    // Now we have the update length, update printers.
    bbb_context_update_printers(context,updc);
    
    // Add voices to output.
    struct bbb_voice *voice=context->voicev;
    int i=context->voicec;
    for (;i-->0;voice++) {
      bbb_voice_update(v,updc,voice);
    }
  
    v+=updc;
    c-=updc;
  }
}

/* Update for multi-channel output.
 * Do a mono update into the same buffer, then expand it.
 */
 
static void bbb_context_update_multi(int16_t *v,int c,struct bbb_context *context) {
  if (c%context->chanc) return;
  int framec=c/context->chanc;
  bbb_context_update_mono(v,framec,context);
  int16_t *dst=v+c;
  const int16_t *src=v+framec;
  if (context->chanc==2) { // 2 is way more likely than any other. Hard-code that case to facilitate optimization.
    while (framec-->0) {
      dst-=2;
      src-=1;
      dst[0]=dst[1]=*src;
    }
  } else {
    while (framec-->0) {
      src--;
      int i=context->chanc;
      while (i-->0) *(--dst)=*src;
    }
  }
}

/* Collect garbage after updating.
 */
 
static void bbb_context_gc(struct bbb_context *context) {

  // Drop any defunct voices from the tail of the list.
  while (context->voicec&&!context->voicev[context->voicec-1].pcm) context->voicec--;
  
  // If we have no addressable voices, reset voiceid_next to 1.
  if (context->voiceid_next!=1) {
    int havevoice=0,i=context->voicec;
    const struct bbb_voice *voice=context->voicev;
    for (;i-->0;voice++) {
      if (voice->voiceid) {
        havevoice=1;
        break;
      }
    }
    if (!havevoice) context->voiceid_next=1;
  }
}

/* Update.
 */

void bbb_context_update(int16_t *v,int c,struct bbb_context *context) {
  if (c<1) return;
  if (!v||!context) return;
  memset(v,0,c<<1);
  if (context->chanc==1) bbb_context_update_mono(v,c,context);
  else bbb_context_update_multi(v,c,context);
  bbb_context_gc(context);
}

/* Begin song.
 */

int bbb_context_play_song(struct bbb_context *context,struct bb_midi_file *file,int repeat) {

  // Do nothing if we're already playing it.
  // There is no "force" option; caller can stop and restart if desired.
  if (file&&context->song&&(file==context->song->file)) return 0;
  
  // Null to end song.
  if (!file) {
    bb_midi_file_reader_del(context->song);
    context->song=0;
    return 0;
  }
  
  // Stop what's playing now.
  if (context->song) {
    bb_midi_file_reader_del(context->song);
    context->song=0;
    bbb_context_all_song_notes_off(context);
  }
  
  // Create a new file reader for it.
  if (!(context->song=bb_midi_file_reader_new(file,context->rate))) return -1;
  context->song->repeat=repeat;

  return 0;
}

/* Pack sndid.
 */
 
uint32_t bbb_sndid(const struct bbb_context *context,uint8_t pid,uint8_t noteid,uint8_t velocity) {

  // No context, give the generic response.
  if (!context) return (pid<<16)|(noteid<<8)|velocity;
  
  // First, try to make pid valid.
  struct bbb_program **pv=context->store->programv;
       if (pv[pid]) ; // as requested
  else if (pv[pid&~0x07]) pid&=~0x07; // start of row 
  else if (pv[pid&~0x7f]) pid&=~0x7f; // start of bank
  // We don't default to zero at the end: 0x80..0xff are presumed to be drums and foley (silence better than a tonal default).
  struct bbb_program *program=pv[pid];
  if (!program) return 0;
  
  // Program takes it from here.
  return bbb_program_pack_sndid(program,pid,noteid,velocity);
}

#include "bb_cli.h"
#include "share/bb_fs.h"
#include "share/bb_codec.h"
#include "share/bb_midi.h"

/* Context.
 */
 
struct mid2bba_context {
  struct bb_encoder dst;
  const char *srcpath;
  const char *dstpath;
  void *midi;
  int midic;
  struct bb_midi_file *midifile;
  struct bb_midi_file_reader *reader;
  int delay; // in frames, 256 frames per bba tick
  uint8_t chid;
};

static void mid2bba_context_cleanup(struct mid2bba_context *ctx) {
  if (ctx->midi) free(ctx->midi);
  bb_encoder_cleanup(&ctx->dst);
  bb_midi_file_del(ctx->midifile);
  bb_midi_file_reader_del(ctx->reader);
}

/* Encode any pending delay and drop to zero in context.
 * Encoded delay is just an 8-bit integer with the high bit clear.
 */
 
static int mid2bba_flush_delay(struct mid2bba_context *ctx) {
  int tickc=ctx->delay>>8;
  ctx->delay&=0xff;
  while (tickc>=0x7f) {
    if (bb_encode_intbe(&ctx->dst,0x7f,1)<0) return -1;
    tickc-=0x7f;
  }
  if (tickc>0) {
    if (bb_encode_intbe(&ctx->dst,tickc,1)<0) return -1;
  }
  return 0;
}

/* Emit note.
 */
 
static int mid2bba_emit_note(struct mid2bba_context *ctx,uint8_t chid,uint8_t noteid,uint8_t velocity) {

  if ((noteid<0x20)||(noteid>=0x60)) {
    //TODO shift offset if necessary
    return 0;
  }
  noteid-=0x20;
  
  if (chid!=ctx->chid) {
    if (bb_encode_intbe(&ctx->dst,0xe0|chid,1)<0) return -1;
    ctx->chid=chid;
  }

  if (bb_encode_intbe(&ctx->dst,0x80|noteid,1)<0) return -1;
  if (bb_encode_intbe(&ctx->dst,0x00|velocity,1)<0) return -1;//XXX not really 0x80|

  return 0;
}

/* Receive event.
 */
 
static int mid2bba_receive_event(struct mid2bba_context *ctx,const struct bb_midi_event *event) {

  // Check for any events we can ignore.
  //TODO
  
  // If a delay is pending, emit it.
  if (mid2bba_flush_delay(ctx)<0) return -1;
  
  // Encode the event.
  //TODO
  //fprintf(stderr,"%s %02x @%02x (%02x,%02x) %d\n",__func__,event->opcode,event->chid,event->a,event->b,event->c);
  switch (event->opcode) {
    case BB_MIDI_OPCODE_NOTE_ON: if (mid2bba_emit_note(ctx,event->chid,event->a,event->b)<0) return -1; break;
    case BB_MIDI_OPCODE_NOTE_OFF: if (mid2bba_emit_note(ctx,event->chid,event->a,0)<0) return -1; break;
  }
  
  return 0;
}

/* Receive delay.
 * We don't encode it right away: Could be that we're going to ignore the next event, and can combine some delays.
 */
 
static int mid2bba_receive_delay(struct mid2bba_context *ctx,int tickc) {
  if (ctx->delay>INT_MAX-tickc) return -1;
  ctx->delay+=tickc;
  return 0;
}

/* Reencode file in memory.
 */
 
static int mid2bba(struct mid2bba_context *ctx) {
  
  if (!(ctx->midifile=bb_midi_file_new(ctx->midi,ctx->midic))) {
    fprintf(stderr,"%s: Failed to parse MIDI file (%d bytes).\n",ctx->srcpath,ctx->midic);
    return -1;
  }
  if (!(ctx->reader=bb_midi_file_reader_new(ctx->midifile,48<<8))) {
    return -1;
  }
  ctx->reader->repeat=0;
  
  // Play the song and capture its output.
  while (1) {
    struct bb_midi_event event={0};
    int framec=bb_midi_file_reader_update(&event,ctx->reader);
    if (framec<0) {
      if (bb_midi_file_reader_is_complete(ctx->reader)) break;
      fprintf(stderr,"%s: Error playing MIDI file.\n",ctx->srcpath);
      return -1;
    }
    if (framec) {
      if (mid2bba_receive_delay(ctx,framec)<0) return -1;
      if (bb_midi_file_reader_advance(ctx->reader,framec)<0) return -1;
    } else {
      if (mid2bba_receive_event(ctx,&event)<0) return -1;
    }
  }
  if (mid2bba_flush_delay(ctx)<0) return -1;
  
  return 0;
}

/* Main.
 */
 
int main_mid2bba(char **argv,int argc) {

  if ((argc!=2)||memcmp(argv[0],"-o",2)||(argv[1][0]=='-')) {
    fprintf(stderr,"Usage: beepbot mid2bba -oOUTPUT INPUT\n");
    return 1;
  }
  struct mid2bba_context ctx={
    .dstpath=argv[0]+2,
    .srcpath=argv[1],
  };
  
  if ((ctx.midic=bb_file_read(&ctx.midi,ctx.srcpath))<0) {
    fprintf(stderr,"%s: Failed to read file.\n",ctx.srcpath);
    mid2bba_context_cleanup(&ctx);
    return 1;
  }
  
  if (mid2bba(&ctx)<0) {
    fprintf(stderr,"%s: Failed to reencode song.\n",ctx.srcpath);
    mid2bba_context_cleanup(&ctx);
    return 1;
  }
  
  if (bb_file_write(ctx.dstpath,ctx.dst.v,ctx.dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file.\n",ctx.dstpath);
    mid2bba_context_cleanup(&ctx);
    return 1;
  }
  
  mid2bba_context_cleanup(&ctx);
  return 0;
}

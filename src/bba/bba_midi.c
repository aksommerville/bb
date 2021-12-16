#include "bba.h"
#include "share/bb_midi.h"

/* MIDI event, main entry point.
 */
 
void bba_midi_event(struct bba_synth *synth,const struct bb_midi_event *event) {
  if (event->chid>=BBA_CHANNEL_COUNT) return;
  struct bba_channel *channel=synth->channelv+event->chid;
  switch (event->opcode) {
  
    case BB_MIDI_OPCODE_PROGRAM: {
    
        // Odd numbered programs are squares, evens are saws.
        if (event->a&0x01) channel->shape=BBA_VOICE_SHAPE_SQUARE;
        else channel->shape=BBA_VOICE_SHAPE_SAW;
        
        // Three bits for release time.
        channel->env.rlsthi=(event->a&0x0e)<<4;
        channel->env.rlsthi|=channel->env.rlsthi>>3;
        channel->env.rlsthi|=channel->env.rlsthi>>6;
        channel->env.rlstlo=channel->env.rlsthi>>1;
        
        // Three bits for attack and sustain.
        channel->env.atktlo=(event->a&0x70)<<1;
        channel->env.atktlo|=channel->env.atktlo>>3;
        channel->env.atktlo|=channel->env.atktlo>>6;
        channel->env.atkthi=channel->env.atktlo>>1;
        channel->env.declhi=0xff-channel->env.atktlo;
        channel->env.decllo=channel->env.declhi>>1;
        channel->env.dectlo=channel->env.atktlo;
        channel->env.decthi=channel->env.atkthi;
        
        // The rest is constant.
        channel->env.flags=BBA_ENV_VELOCITY|BBA_ENV_SUSTAIN;
        channel->env.atkllo=0x80;
        channel->env.atklhi=0xff;
        channel->note0=0x20;
        
        // Finally, release every note on this channel (necessary, in case note0 changed).
        struct bba_voice *voice=synth->voicev;
        int i=synth->voicec;
        for (;i-->0;voice++) {
          voice->env.sustain=0;
        }
        
      } break;
      
    case BB_MIDI_OPCODE_NOTE_ON: bba_synth_note(synth,event->chid,event->a-channel->note0,event->b); break;
    case BB_MIDI_OPCODE_NOTE_OFF: bba_synth_note(synth,event->chid,event->a-channel->note0,0); break;
  }
}

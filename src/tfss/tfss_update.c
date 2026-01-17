#include "tfss_internal.h"

/* Dispatch one event.
 */
 
static inline void tfss_dispatch_event(const struct midifile_event *event) {
  switch (event->opcode) {
    case 0x80: tfss_event_note_off(event->chid,event->a,event->b); break;
    case 0x90: tfss_event_note_on(event->chid,event->a,event->b); break;
    case 0xa0: tfss_event_note_adjust(event->chid,event->a,event->b); break;
    case 0xb0: tfss_event_control(event->chid,event->a,event->b); break;
    case 0xc0: tfss_event_program(event->chid,event->a); break;
    case 0xd0: tfss_event_pressure(event->chid,event->a); break;
    case 0xe0: tfss_event_wheel(event->chid,event->a,event->b); break;
    case 0xff: tfss_event_meta(event->chid,event->a,event->v,event->c); break;
    case 0xf0: case 0xf7: tfss_event_sysex(event->chid,event->v,event->c); break;
    // song_position, song_select, and realtime are not possible in files.
  }
}

/* Fire any ready events.
 * Returns frame count to the next event, but never more than (framecmax).
 */
 
static int tfss_update_events(int framecmax) {
  if (!tfss.midifile.src) return framecmax; // No song. We have free rein.
  
  struct midifile_event event;
  int updc;
  while (!(updc=midifile_next(&event,&tfss.midifile))) {
    tfss_dispatch_event(&event);
  }
  if (updc<=0) {
    if (tfss.midifile.error) {
      fprintf(stderr,"Song error!\n");
      tfss.midifile.src=0;
    } else if (tfss.repeat) {
      fprintf(stderr,"Song repeat.\n");
      midifile_reset(&tfss.midifile);
      return 1;
    } else {
      fprintf(stderr,"Song complete.\n");
      tfss.midifile.src=0;
    }
    return framecmax;
  }
  
  if (updc>framecmax) updc=framecmax;
  midifile_advance(&tfss.midifile,updc);
  return updc;
}

/* Mix one chunk of mono voice output into the main.
 * (src) is mono and (dst) is the global channel count.
 */
 
static inline void tfss_mix_voice_mono(float *dst,const struct tfss_voice *voice,const float *src,int framec) {
  for (;framec-->0;dst++,src++) {
    (*dst)+=(*src)*voice->triml;
  }
}
 
static inline void tfss_mix_voice_stereo(float *dst,const struct tfss_voice *voice,const float *src,int framec) {
  for (;framec-->0;dst+=2,src++) {
    dst[0]+=(*src)*voice->triml;
    dst[1]+=(*src)*voice->trimr;
  }
}
 
static inline void tfss_mix_voice_multi(float *dst,const struct tfss_voice *voice,const float *src,int framec) {
  for (;framec-->0;dst+=tfss.chanc,src++) {
    dst[0]+=(*src)*voice->triml;
    dst[1]+=(*src)*voice->trimr;
  }
}

/* Generate the signal.
 */
 
static void tfss_update_signal_1(float *v,int framec) {
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  switch (tfss.chanc) {
    case 1: {
        for (;i-->0;voice++) {
          if (!voice->update) continue;
          voice->framec+=framec;
          voice->update(tfss.vbuf,framec,voice);
          tfss_mix_voice_mono(v,voice,tfss.vbuf,framec);
        }
      } break;
    case 2: {
        for (;i-->0;voice++) {
          if (!voice->update) continue;
          voice->framec+=framec;
          voice->update(tfss.vbuf,framec,voice);
          tfss_mix_voice_stereo(v,voice,tfss.vbuf,framec);
        }
      } break;
    default: {
        for (;i-->0;voice++) {
          if (!voice->update) continue;
          voice->framec+=framec;
          voice->update(tfss.vbuf,framec,voice);
          tfss_mix_voice_multi(v,voice,tfss.vbuf,framec);
        }
      }
  }
}
 
static void tfss_update_signal(float *v,int framec) {
  while (framec>TFSS_VOICE_BUFFER_SIZE) {
    tfss_update_signal_1(v,TFSS_VOICE_BUFFER_SIZE);
    v+=TFSS_VOICE_BUFFER_SIZE*tfss.chanc;
    framec-=TFSS_VOICE_BUFFER_SIZE;
  }
  if (framec>0) {
    tfss_update_signal_1(v,framec);
  }
  // Drop defunct voices from the end only.
  while (tfss.voicec&&!tfss.voicev[tfss.voicec-1].update) tfss.voicec--;
}

/* Update, main entry point.
 */
 
void tfss_update_internal(float *v,int framec) {
  while (framec>0) {
    int updc=tfss_update_events(framec);
    tfss_update_signal(v,updc);
    v+=updc*tfss.chanc;
    framec-=updc;
  }
}

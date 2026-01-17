#include "tfss_internal.h"

struct tfss tfss={0};

/* Quit.
 */
 
void tfss_quit() {
  tfss.rate=0;
  tfss.chanc=0;
}

/* Rate table.
 */
 
static void tfss_calculate_rate_table() {
  const int refnote=0x45; // A4. Can pick a different reference, but must have at least 11 notes above.
  const float refhz=440.0; // OK to change this (but why...)
  const float TWELFTH_ROOT_TWO=1.0594630943592953f;
  int i;
  float *fp;
  uint32_t *ip;
  
  // First calculate the octave from refnote up.
  // We could just go all the way up and down like this, but I worry about rounding error.
  tfss.step_by_noteid[refnote]=refhz/(float)tfss.rate;
  for (i=11,fp=tfss.step_by_noteid+refnote+1;i-->0;fp++) *fp=fp[-1]*TWELFTH_ROOT_TWO;
  // For those an octave or more above refnote, double the rate at n-12.
  for (i=refnote+12;i<128;i++) tfss.step_by_noteid[i]=tfss.step_by_noteid[i-12]*2.0f;
  // And likewise for the lower ones.
  for (i=refnote;i-->0;) tfss.step_by_noteid[i]=tfss.step_by_noteid[i+12]*0.5f;
  
  // Confirm that our floating-point steps ended up in 0..1/2. Clamp if not.
  for (fp=tfss.step_by_noteid,i=128;i-->0;fp++) {
    if (*fp<0.0f) *fp=0.0f; // ...how?
    else if (*fp>0.5f) *fp=0.5f;
  }
  
  // And finally, scale those floats to unsigned 32-bit integers for simpler wavetable lookup.
  for (fp=tfss.step_by_noteid,ip=tfss.dp_by_noteid,i=128;i-->0;fp++,ip++) {
    *ip=(uint32_t)((*fp)*4294967296.0f);
  }
}

/* Init.
 */

int tfss_init(int rate,int chanc) {
  if (tfss.rate) return -1;
  if ((rate<20)||(rate>200000)) return -1;
  if ((chanc<1)||(chanc>8)) return -1;
  
  tfss.rate=rate;
  tfss.chanc=chanc;
  
  tfss_calculate_rate_table();
  //TODO sine table
  
  return 0;
}

/* Update.
 */

void tfss_update(float *v,int samplec) {
  {
    float *p=v;
    int i=samplec;
    for (;i-->0;p++) *p=0.0f;
  }
  if (tfss.chanc) {
    int framec=samplec/tfss.chanc;
    tfss_update_internal(v,framec);
  }
}

/* Start a raw PCM voice.
 */

void tfss_play_pcm(const float *v,int c,float trim,float pan) {
  fprintf(stderr,"%s c=%d trim=%f pan=%f\n",__func__,c,trim,pan);
  //TODO
}

/* Start a new song.
 */

void tfss_play_song(const void *v,int c,int repeat) {
  fprintf(stderr,"%s c=%d repeat=%d\n",__func__,c,repeat);
  tfss_event_realtime(0xff);
  tfss.repeat=repeat;
  if (v&&(c>0)) {
    if (midifile_init(&tfss.midifile,v,c,tfss.rate)<0) {
      tfss.midifile.src=0;
    }
  } else {
    tfss.midifile.src=0;
  }
}

/* Deliver raw events.
 */
 
static void XXXupd(float *v,int c,struct tfss_voice *voice) {
  for (;c-->0;v++) {
    voice->p+=voice->dp;
    (*v)=(voice->p&0x80000000)?-1.0f:1.0f;
  }
}

void tfss_event_note_on(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  fprintf(stderr,"%s chid=%d note=0x%02x vel=0x%02x\n",__func__,chid,noteid,velocity);
  //XXX Doing this as quick-n-dirty as possible, just to get some output.
  struct tfss_voice *voice=0;
  if (tfss.voicec<TFSS_VOICE_LIMIT) {
    voice=tfss.voicev+tfss.voicec++;
  } else {
    struct tfss_voice *q=tfss.voicev;
    int i=TFSS_VOICE_LIMIT;
    voice=q;
    for (;i-->0;q++) {
      if (!q->update) {
        voice=q;
        break;
      }
      if (q->framec>voice->framec) { // Prefer to evict the oldest.
        voice=q;
      }
    }
  }
  voice->triml=0.100f;
  voice->trimr=0.100f;
  voice->framec=0;
  voice->chid=chid;
  voice->noteid=noteid;
  voice->update=XXXupd;
  voice->p=0;
  voice->dp=tfss.dp_by_noteid[noteid&0x7f];
}

void tfss_event_note_off(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  fprintf(stderr,"%s chid=%d note=0x%02x vel=0x%02x\n",__func__,chid,noteid,velocity);
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    if (!voice->update) continue;
    if (voice->chid!=chid) continue;
    if (voice->noteid!=noteid) continue;
    voice->update=0; // stop cold
  }
}

void tfss_event_note_adjust(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  fprintf(stderr,"%s chid=%d note=0x%02x vel=0x%02x\n",__func__,chid,noteid,velocity);
  //TODO
}

void tfss_event_control(uint8_t chid,uint8_t k,uint8_t v) {
  fprintf(stderr,"%s chid=%d k=0x%02x v=0x%02x\n",__func__,chid,k,v);
  //TODO
}

void tfss_event_program(uint8_t chid,uint8_t pid) {
  fprintf(stderr,"%s chid=%d pid=0x%02x\n",__func__,chid,pid);
  //TODO
}

void tfss_event_pressure(uint8_t chid,uint8_t pressure) {
  fprintf(stderr,"%s chid=%d pressure=0x%02x\n",__func__,chid,pressure);
  //TODO
}

void tfss_event_wheel(uint8_t chid,uint8_t lo,uint8_t hi) {
  fprintf(stderr,"%s chid=%d lo=0x%02x hi=0x%02x\n",__func__,chid,lo,hi);
  //TODO
}

void tfss_event_meta(uint8_t chid,uint8_t type,const void *v,int c) {
  fprintf(stderr,"%s chid=%d type=0x%02x c=%d\n",__func__,chid,type,c);
  //TODO
}

void tfss_event_sysex(uint8_t chid,const void *v,int c) {
  fprintf(stderr,"%s chid=%d c=%d\n",__func__,chid,c);
  //TODO
}

void tfss_event_song_position(uint8_t lo,uint8_t hi) {
  fprintf(stderr,"%s lo=0x%02x hi=0x%02x\n",__func__,lo,hi);
  //TODO
}

void tfss_event_song_select(uint8_t v) {
  fprintf(stderr,"%s 0x%02x\n",__func__,v);
  //TODO
}

void tfss_event_realtime(uint8_t cmd) {
  fprintf(stderr,"%s 0x%02x\n",__func__,cmd);
  //TODO
}

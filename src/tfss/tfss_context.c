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

/* Initialize channels.
 */
 
static void tfss_init_channels() {
  struct tfss_channel *channel=tfss.channelv;
  int chid=0;
  for (;chid<TFSS_CHANNEL_LIMIT;chid++,channel++) {
    channel->chid=chid;
    channel->trim=0.5f*TFSS_TRIM_MAX;
    channel->pan=0.0f;
    channel->note_on=0;
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
  tfss_init_channels();
  
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

/* Find voice.
 */
 
struct tfss_voice *tfss_addressable_voice(uint8_t chid,uint8_t noteid) {
  if (chid>=TFSS_CHANNEL_LIMIT) return 0;
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    if (!voice->update) continue;
    if (voice->chid!=chid) continue;
    if (voice->noteid!=noteid) continue;
    return voice;
  }
  return 0;
}

struct tfss_voice *tfss_get_unused_voice() {
  if (tfss.voicec<TFSS_VOICE_LIMIT) {
    struct tfss_voice *voice=tfss.voicev+tfss.voicec++;
    voice->update=0;
    return voice;
  }
  struct tfss_voice *voice=tfss.voicev;
  struct tfss_voice *q=voice;
  int i=tfss.voicec;
  for (;i-->0;q++) {
    if (!q->update) return q;
    if (q->framec>voice->framec) { // Prefer to evict the older.
      voice=q;
    }
  }
  return voice;
}

/* Reset channels.
 */
 
void tfss_reset_all_channels() {
  struct tfss_channel *channel=tfss.channelv;
  int i=TFSS_CHANNEL_LIMIT;
  for (;i-->0;channel++) {
    channel->pid=0;
    channel->trim=0.5f*TFSS_TRIM_MAX;
    channel->pan=0.0f;
    channel->note_on=0;
  }
}

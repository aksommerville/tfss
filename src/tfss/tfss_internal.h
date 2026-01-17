#ifndef TFSS_INTERNAL_H
#define TFSS_INTERNAL_H

#include "tfss.h"
#include "midifile.h"
#include <stdio.h> /* XXX Very temp, enable logging while I build this out. */

/* Voices update initially to a full-level mono buffer, then we mix to the main output generically.
 * This buffer size is effectively the largest signal update we can do at a time. Too low will impede performance.
 */
#define TFSS_VOICE_BUFFER_SIZE 1024

#define TFSS_VOICE_LIMIT 32

extern struct tfss {
  int rate,chanc; // 0 if uninitialized.
  struct midifile midifile;
  int repeat;
  float vbuf[TFSS_VOICE_BUFFER_SIZE];
  float step_by_noteid[128]; // 0..0.5, how far along a wave to advance at each frame, for a given noteid.
  uint32_t dp_by_noteid[128]; // Same but scaled to 0..UINT_MAX.
  
  struct tfss_voice {
    float triml,trimr; // Only (triml) if mono.
    int framec; // Incremented generically by the mixer.
    uint8_t chid,noteid; // (0xff,0xff) if not addressable.
    void (*update)(float *v,int c,struct tfss_voice *voice); // Must overwrite. Null to kill a voice.
    uint32_t p;
    uint32_t dp;
  } voicev[TFSS_VOICE_LIMIT];
  int voicec;
  
} tfss;

/* Generate so many frames of output, and advance clocks by the same.
 * Caller must zero (v) first.
 */
void tfss_update_internal(float *v,int framec);

#endif

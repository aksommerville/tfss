#ifndef TFSS_INTERNAL_H
#define TFSS_INTERNAL_H

#include "tfss.h"
#include "midifile.h"
#include <stdio.h> /* XXX Very temp, enable logging while I build this out. */

/* Voices update initially to a full-level mono buffer, then we mix to the main output generically.
 * This buffer size is effectively the largest signal update we can do at a time. Too low will impede performance.
 */
#define TFSS_VOICE_BUFFER_SIZE 1024

#define TFSS_VOICE_LIMIT 32 /* Arbitrary. */
#define TFSS_CHANNEL_LIMIT 16 /* Inherited from MIDI. Don't change. */
#define TFSS_TRIM_MAX 0.200f /* Arbitrary, but keep under 1. */
#define TFSS_VOICE_FV_SIZE 4
#define TFSS_VOICE_IV_SIZE 4
#define TFSS_VOICE_PV_SIZE 4
#define TFSS_CHANNEL_FV_SIZE 4
#define TFSS_CHANNEL_IV_SIZE 4
#define TFSS_CHANNEL_PV_SIZE 4

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
    
    /* The (update) hook also serves as our "defunct" flag.
     * It must overwrite the entire provided buffer at full volume. Caller applies trim and pan.
     * (release) is optional but highly recommended. If unset, voices stop cold at release.
     */
    void (*update)(float *v,int c,struct tfss_voice *voice);
    void (*release)(struct tfss_voice *voice,uint8_t velocity);
    
    // Everything else, usage depends on (update):
    uint32_t p;
    uint32_t dp;
    float fv[TFSS_VOICE_FV_SIZE];
    int iv[TFSS_VOICE_IV_SIZE];
    void *pv[TFSS_VOICE_PV_SIZE];
  } voicev[TFSS_VOICE_LIMIT];
  int voicec;
  
  struct tfss_channel {
    uint8_t chid; // Same as my index.
    uint8_t pid; // Per Program Change. We'll ignore Bank Select.
    float trim; // 0..1, per Control 0x07 Volume MSB and TFSS_TRIM_MAX.
    float pan; // -1..0..1, per Control 0x0a Pan MSB.
    // No note_off; that lives in each voice.
    void (*note_on)(struct tfss_channel *channel,struct tfss_voice *voice,uint8_t noteid,uint8_t velocity); // Set (chid,noteid) if addressable.
    void (*note_adjust)(struct tfss_channel *channel,struct tfss_voice *voice,uint8_t velocity);
    void (*control)(struct tfss_channel *channel,uint8_t k,uint8_t v);
    void (*pressure)(struct tfss_channel *channel,uint8_t v); // Falls back to note_adjust if unset.
    void (*wheel)(struct tfss_channel *channel,float vnorm); // -1..1, channel is responsible for updating addressable voices.
    // Some general-purpose fields for the programs' use:
    float fv[TFSS_CHANNEL_FV_SIZE];
    int iv[TFSS_CHANNEL_IV_SIZE];
    void *pv[TFSS_CHANNEL_PV_SIZE];
  } channelv[TFSS_CHANNEL_LIMIT];
  
} tfss;

/* Generate so many frames of output, and advance clocks by the same.
 * Caller must zero (v) first.
 */
void tfss_update_internal(float *v,int framec);

void tfss_stop_all_voices();
void tfss_release_all_voices();
struct tfss_voice *tfss_addressable_voice(uint8_t chid,uint8_t noteid);
struct tfss_voice *tfss_get_unused_voice(); // => May be null, and may be inuse but evictable.
void tfss_reset_all_channels();

/* Program types.
 * - trivial
 * - - ctl 0x46: lo trim = 0.5
 * - - ctl 0x47: hi trim = 1
 * - - ctl 0x48: release time, sec = 0.1
 */
void tfss_trivial_init(struct tfss_channel *channel);

#endif

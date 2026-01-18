#include "tfss_internal.h"

/* Rebuild channel based on its (pid).
 * Never fails to set (note_on).
 */
 
static void tfss_channel_require(struct tfss_channel *channel) {
  channel->note_on=0;
  channel->note_adjust=0;
  channel->control=0;
  channel->pressure=0;
  channel->wheel=0;
  switch (channel->pid) {
    //TODO
  }
  if (!channel->note_on) tfss_trivial_init(channel);
}

/* Note On.
 */

void tfss_event_note_on(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  if (!channel->note_on) tfss_channel_require(channel);
  struct tfss_voice *voice=tfss_get_unused_voice();
  if (!voice) return;
  
  voice->triml=channel->trim;
  if (tfss.chanc>1) {
    voice->trimr=voice->triml;
         if (channel->pan>0.0f) voice->triml*=(1.0f-channel->pan);
    else if (channel->pan<0.0f) voice->trimr*=(1.0f+channel->pan);
  }
  voice->framec=0;
  voice->chid=voice->noteid=0xff;
  voice->update=0;
  voice->release=0;
  
  channel->note_on(channel,voice,noteid,velocity);
  if (!voice->update) return;
}

/* Note Off.
 */

void tfss_event_note_off(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    if (!voice->update) continue;
    if (voice->chid!=chid) continue;
    if (voice->noteid!=noteid) continue;
    if (voice->release) {
      voice->release(voice,velocity);
      voice->chid=voice->noteid=0xff;
    } else {
      voice->update=0;
    }
  }
}

/* Control Change.
 */

void tfss_event_control(uint8_t chid,uint8_t k,uint8_t v) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  
  // Some keys are processed generically.
  switch (k) {
    // Changing a channel's trim or pan does not affect running notes.
    // We could do that, and maybe we should. But it wouldn't touch unaddressable voices, so might be kind of weird.
    // I feel making it only affect future voices is the most consistent approach.
    case 0x07: channel->trim=((float)v*TFSS_TRIM_MAX)/127.0f; return; // Volume MSB
    case 0x0a: channel->pan=(v-0x40)/128.0f; return; // Pan MSB
  }
  
  if (!channel->control) return;
  channel->control(channel,k,v);
}

/* Program Change.
 */

void tfss_event_program(uint8_t chid,uint8_t pid) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  if (channel->pid==pid) return;
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    if (!voice->update) continue;
    if (voice->chid!=chid) continue;
    if (voice->release) {
      voice->release(voice,0x40);
    } else {
      voice->update=0;
      voice->chid=voice->noteid=0xff;
    }
  }
  channel->pid=pid;
  channel->note_on=0; // Signal that we need rebuilt.
}

/* Pitch Wheel.
 */

void tfss_event_wheel(uint8_t chid,uint8_t lo,uint8_t hi) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  if (!channel->wheel) return;
  int v=(lo|(hi<<7))-0x4000;
  float norm=(float)v/16384.0f;
  channel->wheel(channel,norm);
}

/* Realtime: Probably the only one we care about is 0xff System Reset.
 */
 
void tfss_stop_all_voices() {
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    voice->update=0;
  }
  tfss.voicec=0;
}
 
void tfss_release_all_voices() {
  struct tfss_voice *voice=tfss.voicev;
  int i=tfss.voicec;
  for (;i-->0;voice++) {
    if (!voice->update) continue;
    if (voice->release) {
      voice->release(voice,0x40);
      voice->chid=voice->noteid=0xff;
    } else {
      voice->update=0;
    }
  }
}

void tfss_event_realtime(uint8_t cmd) {
  switch (cmd) {
    case 0xff: { // System Reset.
        tfss_stop_all_voices();
        tfss_reset_all_channels();
      } break;
  }
}

/* Aftertouch etc, I don't expect to use these.
 * Maybe let channels implement.
 */

void tfss_event_note_adjust(uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  if (!channel->note_adjust) return;
  struct tfss_voice *voice=tfss_addressable_voice(chid,noteid);
  if (!voice) return;
  channel->note_adjust(channel,voice,velocity);
}

void tfss_event_pressure(uint8_t chid,uint8_t pressure) {
  if (chid>=TFSS_CHANNEL_LIMIT) return;
  struct tfss_channel *channel=tfss.channelv+chid;
  if (channel->pressure) {
    channel->pressure(channel,pressure);
  } else if (channel->note_adjust) {
    struct tfss_voice *voice=tfss.voicev;
    int i=tfss.voicec;
    for (;i-->0;voice++) {
      if (!voice->update) continue;
      if (voice->chid!=chid) continue;
      channel->note_adjust(channel,voice,pressure);
    }
  }
}

void tfss_event_meta(uint8_t chid,uint8_t type,const void *v,int c) {
}

void tfss_event_sysex(uint8_t chid,const void *v,int c) {
}

void tfss_event_song_position(uint8_t lo,uint8_t hi) {
}

void tfss_event_song_select(uint8_t v) {
}

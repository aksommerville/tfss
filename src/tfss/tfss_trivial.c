#include "tfss_internal.h"

#define TRIMLO channel->fv[0]
#define TRIMHI channel->fv[1]
#define RELEASE_TIME channel->iv[0]
#define LEVEL voice->fv[0]
#define DLEVEL voice->fv[1]
#define TTL voice->iv[0]
#define RELEASING voice->iv[1]

/* Update.
 */
 
static void tfss_trivial_update(float *v,int c,struct tfss_voice *voice) {
  if (RELEASING) {
    for (;c-->0;v++) {
      TTL--;
      if (TTL<=0) {
        voice->update=0;
        *v=0.0f;
      } else {
        if ((LEVEL+=DLEVEL)<=0.0f) LEVEL=0.0f;
        voice->p+=voice->dp;
        (*v)=(voice->p&0x80000000)?-LEVEL:LEVEL;
      }
    }
  } else {
    for (;c-->0;v++) {
      voice->p+=voice->dp;
      (*v)=(voice->p&0x80000000)?-LEVEL:LEVEL;
    }
  }
}

/* Note Off.
 */
 
static void tfss_trivial_release(struct tfss_voice *voice,uint8_t velocity) {
  RELEASING=1;
}

/* Note On.
 */
 
static void tfss_trivial_note_on(struct tfss_channel *channel,struct tfss_voice *voice,uint8_t noteid,uint8_t velocity) {
  voice->chid=channel->chid;
  voice->noteid=noteid;
  voice->update=tfss_trivial_update;
  voice->p=0;
  voice->dp=tfss.dp_by_noteid[noteid];
  float fvel=velocity/127.0f;
  LEVEL=(TRIMLO*(1.0f-fvel))+TRIMHI*fvel;
  voice->release=tfss_trivial_release;
  TTL=RELEASE_TIME;
  DLEVEL=-LEVEL/(float)TTL;
  RELEASING=0;
}

/* Control Change.
 */
 
static void tfss_trivial_control(struct tfss_channel *channel,uint8_t k,uint8_t v) {
  switch (k) {
    case 0x46: TRIMLO=v/127.0f; break; // Controller 1 (Sound Variation). Trim at velocity zero.
    case 0x47: TRIMHI=v/127.0f; break; // Controller 2 (Timbre). Trim at velocity one.
    case 0x48: RELEASE_TIME=(v*tfss.rate)>>7; break; // Controller 3 (Release Time). 0..1 s.
  }
}

/* Init.
 */
 
void tfss_trivial_init(struct tfss_channel *channel) {
  channel->note_on=tfss_trivial_note_on;
  channel->control=tfss_trivial_control;
  TRIMLO=0.5f;
  TRIMHI=1.0f;
  RELEASE_TIME=tfss.rate/10;
}

#include "midifile.h"
#include <limits.h>

#define CHUNKID_MThd (('M'<<24)|('T'<<16)|('h'<<8)|'d')
#define CHUNKID_MTrk (('M'<<24)|('T'<<16)|('r'<<8)|'k')

/* VLQ.
 */
 
static int midifile_read_vlq(int *dst,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  if (!(src[0]&0x80)) {
    *dst=src[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(src[1]&0x80)) {
    *dst=((src[0]&0x7f)<<7)|src[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(src[2]&0x80)) {
    *dst=((src[0]&0x7f)<<14)|((src[1]&0x7f)<<7)|src[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(src[3]&0x80)) {
    *dst=((src[0]&0x7f)<<21)|((src[1]&0x7f)<<14)|((src[2]&0x7f)<<7)|src[3];
    return 4;
  }
  return -1;
}

/* Tempo.
 */

// Recalculate (framespertick) from (division,usperqnote,rate).
static void midifile_poke_tempo(struct midifile *midifile) {
  if ((midifile->division<1)||(midifile->usperqnote<1)||(midifile->rate<1)) {
    midifile->framespertick=1000.0;
  } else {
    double sperqnote=(double)midifile->usperqnote/1000000.0;
    double framesperqnote=sperqnote*(double)midifile->rate;
    midifile->framespertick=framesperqnote/(double)midifile->division;
    if (midifile->framespertick<1.0) midifile->framespertick=1.0;
  }
}

static int midifile_frames_from_ticks(const struct midifile *midifile,int tickc) {
  if (tickc<1) return 0;
  int framec=(int)(tickc*midifile->framespertick);
  if (framec<1) return 1;
  return framec;
}

/* Receive MThd chunk.
 */
 
static int midifile_decode_MThd(struct midifile *midifile,const uint8_t *src,int srcc) {
  if (midifile->division) return -1; // Multiple MThd
  if (srcc<6) return -1;
  int format=(src[0]<<8)|src[1];
  int trackc=(src[2]<<8)|src[3];
  int division=(src[4]<<8)|src[5];
  // (format) should be 1 but we'll let anything pass.
  // (trackc) I have no idea what it means.
  // (division) must be in 1..32767, and this matters. It also serves as our "MThd present" flag.
  if ((division<1)||(division>=0x8000)) return -1;
  midifile->division=division;
  return 0;
}

/* Receive MTrk chunk.
 * Despite the name, we're not actually decoding anything at this time.
 */
 
static int midifile_decode_MTrk(struct midifile *midifile,const uint8_t *src,int srcc) {
  if (midifile->trackc>=MIDIFILE_TRACK_LIMIT) return -1;
  struct midifile_track *track=midifile->trackv+midifile->trackc++;
  track->status=0;
  track->chpfx=0xff;
  track->v=src;
  track->c=srcc;
  track->p=0;
  track->delay=-1;
  return 0;
}

/* Decode in a fresh context.
 * (src,srcc) must be set, and (trackc,division) must be zero.
 */
 
static int midifile_decode(struct midifile *midifile) {
  int srcp=0;
  const uint8_t *v=midifile->src;
  for (;;) {
    if (srcp>midifile->srcc-8) break;
    int chunkid=(v[srcp]<<24)|(v[srcp+1]<<16)|(v[srcp+2]<<8)|v[srcp+3]; srcp+=4;
    int chunklen=(v[srcp]<<24)|(v[srcp+1]<<16)|(v[srcp+2]<<8)|v[srcp+3]; srcp+=4;
    if ((chunklen<0)||(srcp>midifile->srcc-chunklen)) return -1;
    const uint8_t *chunk=v+srcp;
    srcp+=chunklen;
    switch (chunkid) {
      case CHUNKID_MThd: if (midifile_decode_MThd(midifile,chunk,chunklen)<0) return -1; break;
      case CHUNKID_MTrk: if (midifile_decode_MTrk(midifile,chunk,chunklen)<0) return -1; break;
    }
  }
  if (!midifile->division) return -1; // Missing MThd
  if (!midifile->trackc) return -1; // No MTrk. Not sure if this should strictly count as an error, but the file is definitely not useful.
  midifile_poke_tempo(midifile);
  return 0;
}

/* Init.
 */
 
int midifile_init(struct midifile *midifile,const void *src,int srcc,int rate) {
  if (!src) return -1;
  if ((rate<20)||(rate>200000)) return -1;
  
  midifile->src=src;
  midifile->srcc=srcc;
  midifile->rate=rate;
  midifile->trackc=0;
  midifile->division=0;
  midifile->error=0;
  midifile->usperqnote=500000;
  
  return midifile_decode(midifile);
}

/* Start over from the beginning.
 */
 
void midifile_reset(struct midifile *midifile) {
  if (!midifile->division) return;
  midifile->error=0;
  midifile->usperqnote=500000;
  struct midifile_track *track=midifile->trackv;
  int i=midifile->trackc;
  for (;i-->0;track++) {
    track->p=0;
    track->status=0;
    track->delay=-1;
    track->chpfx=0xff;
  }
  midifile_poke_tempo(midifile);
}

/* Read delay for a track.
 */
 
static int midifile_track_read_delay(struct midifile *midifile,struct midifile_track *track) {
  int err=midifile_read_vlq(&track->delay,track->v+track->p,track->c-track->p);
  if (err<=0) return -1;
  track->p+=err;
  track->delay=midifile_frames_from_ticks(midifile,track->delay);
  return 0;
}

/* Read event from a track.
 */
 
static int midifile_track_read_event(struct midifile_event *event,struct midifile *midifile,struct midifile_track *track) {
  track->delay=-1;
  if (track->p>=track->c) return -1;
  uint8_t lead=track->v[track->p];
  if (lead&0x80) track->p++;
  else if (track->status) lead=track->status;
  else return -1;
  track->status=lead;
  event->trackp=track-midifile->trackv;
  event->chid=lead&0x0f;
  event->opcode=lead&0xf0;
  switch (event->opcode) {
    case 0x90: { // Note On: Resolve the velocity-zero trick.
        if (track->p>track->c-2) return -1;
        event->a=track->v[track->p++];
        event->b=track->v[track->p++];
        if (!event->b) {
          event->opcode=0x80;
          event->b=0x40;
        }
      } break;
    case 0x80:
    case 0xa0:
    case 0xb0:
    case 0xe0: { // Note Off, Note Adjust, Control, Wheel: Generic 2-byte events.
        if (track->p>track->c-2) return -1;
        event->a=track->v[track->p++];
        event->b=track->v[track->p++];
      } break;
    case 0xc0:
    case 0xd0: { // Program, Pressure: Generic 1-byte events.
        if (track->p>track->c-1) return -1;
        event->a=track->v[track->p++];
      } break;
    case 0xf0: {
        event->chid=track->chpfx;
        event->opcode=lead;
        track->status=0;
        if (lead==0xff) {
          if (track->p>=track->c) return -1;
          event->a=track->v[track->p++];
        }
        int err,len;
        if ((err=midifile_read_vlq(&len,track->v+track->p,track->c-track->p))<1) return -1;
        track->p+=err;
        if (track->p>track->c-len) return -1;
        event->v=track->v+track->p;
        event->c=len;
        track->p+=len;
      } break;
    default: return -1;
  }
  return 0;
}

/* Process events locally.
 * We can't modify or suppress them; caller should see every event just as it is.
 * But tempo and channel prefix, we need to note them.
 */
 
static int midifile_local_event(struct midifile *midifile,struct midifile_track *track,const struct midifile_event *event) {
  const uint8_t *v=event->v;
  switch (event->opcode) {
    case 0xff: switch (event->a) {
        case 0x20: { // MIDI Channel Prefix
            if (event->c>=1) track->chpfx=v[0];
          } break;
        case 0x51: { // Set Tempo
            if (event->c>=3) {
              midifile->usperqnote=(v[0]<<16)|(v[1]<<8)|v[2];
              midifile_poke_tempo(midifile);
            }
          } break;
      } break;
  }
  return 0;
}

/* Next event or delay.
 */

int midifile_next(struct midifile_event *event,struct midifile *midifile) {
  if (!midifile||!midifile->division||midifile->error) return -1;
  struct midifile_track *track;
  int i;
  
  /* Acquire delays if needed, and track the shortest delay.
   * If we find a track with delay zero, pop an event off it and we're done.
   */
  int bestdelay=INT_MAX,anydelay=0;
  for (track=midifile->trackv,i=midifile->trackc;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) {
      if (midifile_track_read_delay(midifile,track)<0) {
        return midifile->error=-1;
      }
      if (track->p>=track->c) continue;
    }
    if (!track->delay) {
      if (midifile_track_read_event(event,midifile,track)<0) {
        return midifile->error=-1;
      }
      if (midifile_local_event(midifile,track,event)<0) {
        return midifile->error=-1;
      }
      return 0;
    }
    if (track->delay<bestdelay) {
      bestdelay=track->delay;
      anydelay=1;
    }
  }
  
  /* All tracks completed? Cool.
   */
  if (!anydelay) {
    return -1;
  }
  return bestdelay;
}

/* Advance clock.
 */

void midifile_advance(struct midifile *midifile,int framec) {
  if (framec<1) return;
  struct midifile_track *track=midifile->trackv;
  int i=midifile->trackc;
  for (;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) {
      if (midifile_track_read_delay(midifile,track)<0) {
        midifile->error=-1;
        return;
      }
      if (track->p>=track->c) continue;
    }
    if (track->delay<framec) {
      midifile->error=-1;
      return;
    }
    track->delay-=framec;
  }
}

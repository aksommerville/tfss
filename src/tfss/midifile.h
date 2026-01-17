/* midifile.h
 * Helper for turning a MIDI file into a stream of events.
 * As this is for tfss, we are strictly dependency-free.
 * That means our input buffer must be borrowed, and we have a hard-coded track limit.
 * We're completely independent of tfss tho, if you want to use this elsewhere it should just drop in.
 */
 
#ifndef MIDIFILE_H
#define MIDIFILE_H

#include <stdint.h>

#define MIDIFILE_TRACK_LIMIT 16

struct midifile {
  const uint8_t *src; // WEAK
  int srcc;
  struct midifile_track {
    uint8_t status;
    uint8_t chpfx;
    const uint8_t *v; // WEAK, within (src).
    int c;
    int p;
    int delay; // <0 if needs read. Frames, not ticks.
  } trackv[MIDIFILE_TRACK_LIMIT];
  int trackc;
  int rate;
  int division;
  int usperqnote; // Meta 0x51 Set Tempo, or the default 500000.
  double framespertick;
  int error; // Nonzero if we failed due to malformed data. Zero if we hit EOF. (after midifile_next() fails).
};

/* Wipe (midifile) and split chunks from (src).
 * Fails if (src) is incorrectly chunked, usually means it's not a MIDI file.
 * Also fails if there are too many MTrk chunks. Bump MIDIFILE_TRACK_LIMIT if needed.
 * Does not fail for malformed track content; we don't read them at this point.
 * We expect format 1, but will accept any format.
 * Division must be in ticks/qnote, not SMPTE.
 * You must supply (rate) in Hertz. This will be used for all time reporting.
 */
int midifile_init(struct midifile *midifile,const void *src,int srcc,int rate);

/* Start over from the beginning.
 */
void midifile_reset(struct midifile *midifile);

struct midifile_event {
  uint8_t trackp; // Index of MTrk chunk it came from. Usually not interesting.
  uint8_t chid; // 0..15 or 0xff if channelless. NB MIDI convention is one-based but we report zero-based as on the wire.
  uint8_t opcode; // Typically the high 4 bits nonzero and low 4 zero. Can be 0xff for Meta or 0xf0 for Sysex.
  uint8_t a,b; // Data bytes, per opcode. For 0xff Meta, (a) is the type.
  const void *v; // Payload for Meta and Sysex.
  int c; // Length of payload.
};

/* If an event is pending, populate (event) and return zero.
 * No events ready, we return the frame count to the next event.
 * Errors are possible, we'll return <0.
 * Also <0 at successful EOF. Check (midifile->error) to distinguish EOF from real errors.
 */
int midifile_next(struct midifile_event *event,struct midifile *midifile);

/* After nexting out all the pending events, call this to advance time.
 * (framec) should be no more than the last thing returned by midifile_next().
 */
void midifile_advance(struct midifile *midifile,int framec);

#endif

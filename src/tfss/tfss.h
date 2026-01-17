/* tfss.h
 * Trivial Finite-State Synthesizer, public interface.
 */
 
#ifndef TFSS_H
#define TFSS_H

#include <stdint.h>

/* Clean up all globals such that we can init again after.
 * If you're terminating, there's no need to call this.
 */
void tfss_quit();

/* Start up.
 * Fails if (rate,chanc) unacceptable, or if already initialized.
 * We'll run at exactly the rate and channel count you specify, no negotiation.
 * <0 on error, 0 on success.
 */
int tfss_init(int rate,int chanc);

/* Advance time and generate the main output signal.
 * (samplec) must be a multiple of the (chanc) you initialized with.
 * Emits channels in order each frame: [left,right,silent,...,silent] up to (chanc).
 */
void tfss_update(float *v,int samplec);

/* Start playing a raw PCM buffer at the next update.
 * You must hold (v) constant until it's done playing.
 */
void tfss_play_pcm(const float *v,int c,float trim,float pan);

/* Drop the current song and begin a new one.
 * (v,c) is a MIDI file. You must hold it constant until it's done playing.
 * (0,0,0) to play silence.
 */
void tfss_play_song(const void *v,int c,int repeat);

/* Deliver MIDI events just as if the song had produced them.
 * Wheel takes two seven-bit integers, little end first, just as on the wire.
 * Meta and Sysex both take an optional (chid). From Meta 0x20 MIDI Channel Prefix if you have it.
 * I don't expect to use release velocity, aftertouch, or sysex, they're just here for completeness.
 * Realtime events are a single byte 0xf6..0xff. These are not possible in files, only streams.
 */
void tfss_event_note_on(uint8_t chid,uint8_t noteid,uint8_t velocity);
void tfss_event_note_off(uint8_t chid,uint8_t noteid,uint8_t velocity);
void tfss_event_note_adjust(uint8_t chid,uint8_t noteid,uint8_t velocity);
void tfss_event_control(uint8_t chid,uint8_t k,uint8_t v);
void tfss_event_program(uint8_t chid,uint8_t pid);
void tfss_event_pressure(uint8_t chid,uint8_t pressure);
void tfss_event_wheel(uint8_t chid,uint8_t lo,uint8_t hi);
void tfss_event_meta(uint8_t chid,uint8_t type,const void *v,int c);
void tfss_event_sysex(uint8_t chid,const void *v,int c);
void tfss_event_song_position(uint8_t lo,uint8_t hi); // 0xf2
void tfss_event_song_select(uint8_t v); // 0xf3
void tfss_event_realtime(uint8_t cmd);

#endif

/* pulse_internal.h
 * Link: -lpulse-simple
 * A tfss wrapper that output via PulseAudio.
 * Take input either from a MIDI file, or an OSS-style MIDI device.
 */
 
#ifndef PULSE_INTERNAL_H
#define PULSE_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include "tfss/tfss.h"

struct pulse;

/* For the wrapper app (start at main.c).
 **************************************************************************/

extern struct g {
  const char *exename;
  const char *srcpath; // REQUIRED. "-" for stdin.
  int rate;
  int chanc;
  int buffer;
  int repeat; // Irrelevant for streaming.
  
  volatile int sigc;
  struct pulse *pulse;
  volatile int clip;
  int srcfd;
  void *src; // Null if input is a stream, otherwise it's a regular file (MIDI).
  int srcc;
  uint8_t rstat; // Running Status in stream mode. (is there such a thing?)
} g;

/* For the PulseAudio driver copied from egg2 (pulse_context.c).
 **************************************************************************/

struct pulse_delegate {
  void *userdata;
  
  /* You must write (c) samples to (v).
   * (c) is in samples as usual -- not frames, not bytes.
   */
  void (*pcm_out)(int16_t *v,int c,void *userdata);
};

struct pulse_setup {
  int rate; // hz
  int chanc; // usually 1 or 2
  int buffersize; // Hardware buffer size in frames. Usually best to leave it zero, let us decide.
  const char *appname;
  const char *servername;
};

struct pulse;

void pulse_del(struct pulse *pulse);

struct pulse *pulse_new(
  const struct pulse_delegate *delegate,
  const struct pulse_setup *setup
);

void *pulse_get_userdata(const struct pulse *pulse);
int pulse_get_rate(const struct pulse *pulse);
int pulse_get_chanc(const struct pulse *pulse);
int pulse_get_running(const struct pulse *pulse);

void pulse_set_running(struct pulse *pulse,int running);

int pulse_update(struct pulse *pulse);
int pulse_lock(struct pulse *pulse);
void pulse_unlock(struct pulse *pulse);

double pulse_estimate_remaining_buffer(const struct pulse *pulse);

struct pulse {
  struct pulse_delegate delegate;
  int rate,chanc,running;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int16_t *buf;
  int bufa; // samples
  pa_simple *pa;
  int64_t buffer_time_us;
  double buftime_s;
};

int64_t pulse_now();

#endif

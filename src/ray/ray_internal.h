#ifndef RAY_INTERNAL_H
#define RAY_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include "tfss/tfss.h"
#include "raylib.h"

extern struct g {
  const char *exename;
  const char *srcpath; // REQUIRED. "-" for stdin.
  int rate;
  int chanc;
  int repeat; // Irrelevant for streaming.
  
  volatile int sigc;
  volatile int clip;
  int srcfd;
  void *src; // Null if input is a stream, otherwise it's a regular file (MIDI).
  int srcc;
  uint8_t rstat; // Running Status in stream mode. (is there such a thing?)
  
  AudioStream stream;
} g;

extern const unsigned char _embedded_song[];
extern const int _embedded_song_size;

#endif

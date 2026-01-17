#include "ray_internal.h"
#include <signal.h>

struct g g={0};

/* Signal handler.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(g.sigc)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",g.exename);
        exit(1);
      } break;
  }
}

/* Callback from Raylib's AudioStream.
 * TODO It looks like we're allowed to pass floats instead. We could skip the quantization here.
 */
 
static void quantize_output(int16_t *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) {
    int n=(int)((*src)*32767.0f);
    if (n<-32768) { *dst=-32768; g.clip++; }
    else if (n>32767) { *dst=32767; g.clip++; }
    else *dst=n;
  }
}
 
static void cb_pcm_out(void *v,unsigned int framec) {
  #define QBUFLIMIT 1024
  float qbuf[QBUFLIMIT];
  int qbufframec=QBUFLIMIT/g.chanc;
  while (framec>0) {
    int updc=framec;
    if (updc>qbufframec) updc=qbufframec;
    tfss_update(qbuf,updc*g.chanc);
    quantize_output(v,qbuf,updc*g.chanc);
    v+=updc*g.chanc;
    framec-=updc;
  }
  #undef QBUFLIMIT
}

/* Read regular file. Caller opens and closes it.
 */
 
static int rdfile(void *dstpp,int fd) {
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) return -1;
  char *dst=malloc(flen?flen:1);
  if (!dst) return -1;
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  *(void**)dstpp=dst;
  return dstc;
}

/* Evaluate unsigned integer for argv.
 * Decimal only, let's keep things simple.
 */
 
static int rcvarg_uint(int *dst,const char *src,int srcc,const char *name) {
  if (src&&(srcc>0)) {
    *dst=0;
    int srcp=0;
    for (;srcp<srcc;srcp++) {
      int digit=src[srcp++];
      if ((digit<'0')||(digit>'9')) goto _invalid_;
      digit-='0';
      if (*dst>UINT_MAX/10) goto _invalid_;
      (*dst)*=10;
      if (*dst>UINT_MAX-digit) goto _invalid_;
      (*dst)+=digit;
    }
    return 0;
  }
 _invalid_:;
  fprintf(stderr,"%s: Expected positive decimal integer for '%s', found '%.*s'\n",g.exename,name,srcc,src);
  return -2;
}

/* Receive one argument.
 */
 
static int rcvarg(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  
  // If (v) is null, not just empty, replace it with "1" or "0".
  if (!v) {
    if ((kc>3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
    } else {
      v="1";
    }
    vc=1;
  } else if (vc<0) {
    vc=0;
    while (v[vc]) vc++;
  }
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    fprintf(stdout,
      "\n"
      "Usage: %s [OPTIONS] FILE\n"
      "\n"
      "FILE may be a MIDI file, OSS-like MIDI device, or '-' for stdin (MIDI stream or file, we detect).\n"
      "\n"
      "OPTIONS:\n"
      "  --help           Print this message.\n"
      "  --rate=44100     Output rate in Hertz.\n"
      "  --chanc=2        Channel count.\n"
      "  --stereo         Alias for '--chanc=2'.\n"
      "  --mono           Alias for '--chanc=1'.\n"
      "  --repeat         Play file on repeat. Irrelevant for streams.\n"
      "\n"
    ,g.exename);
    exit(0);
  }
  
  if ((kc==4)&&!memcmp(k,"rate",4)) return rcvarg_uint(&g.rate,v,vc,"rate");
  if ((kc==5)&&!memcmp(k,"chanc",5)) return rcvarg_uint(&g.chanc,v,vc,"chanc");
  if ((kc==6)&&!memcmp(k,"stereo",6)) { g.chanc=2; return 0; }
  if ((kc==4)&&!memcmp(k,"mono",4)) { g.chanc=1; return 0; }
  if ((kc==6)&&!memcmp(k,"repeat",6)) return rcvarg_uint(&g.repeat,v,vc,"repeat");
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",g.exename,kc,k,vc,v);
  return -2;
}

/* Consume one event from a MIDI stream and deliver it to synth.
 * Returns length consumed on success; zero is an error.
 * Driver must be locked.
 */
 
static int stream_update_1(const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  int srcp=0;
  uint8_t lead=src[srcp];
  if (lead&0x80) srcp++;
  else if (g.rstat&0x80) lead=g.rstat;
  else {
    fprintf(stderr,"%s: Unexpected leading byte 0x%02x from MIDI stream.\n",g.exename,lead);
    return -2;
  }
  if ((lead&0xf0)!=0xf0) g.rstat=lead;
  switch (lead&0xf0) {
    #define ARGA if (srcp>srcc-1) return -1; uint8_t a=src[srcp++];
    #define ARGAB if (srcp>srcc-2) return -1; uint8_t a=src[srcp++]; uint8_t b=src[srcp++];
    case 0x80: { ARGAB tfss_event_note_off(lead&0x0f,a,b); } break;
    case 0x90: { ARGAB if (b) tfss_event_note_on(lead&0x0f,a,b); else tfss_event_note_off(lead&0x0f,a,0x40); } break;
    case 0xa0: { ARGAB tfss_event_note_adjust(lead&0x0f,a,b); } break;
    case 0xb0: { ARGAB tfss_event_control(lead&0x0f,a,b); } break;
    case 0xc0: { ARGA tfss_event_program(lead&0x0f,a); } break;
    case 0xd0: { ARGA tfss_event_pressure(lead&0x0f,a); } break;
    case 0xe0: { ARGAB tfss_event_wheel(lead&0x0f,a,b); } break;
    case 0xf0: switch (lead) {
        case 0xf0: fprintf(stderr,"%s: Sysex not supported.\n",g.exename); return -1; // too complicated, and no use for it.
        case 0xf2: { ARGAB tfss_event_song_position(a,b); } break;
        case 0xf3: { ARGA tfss_event_song_select(a); } break;
        default: tfss_event_realtime(lead); break;
      } break;
    #undef ARGA
    #undef ARGAB
    default: return -1;
  }
  return srcp;
}

/* Called when (g.srcfd) has polled.
 * Lock the driver, read from the input, digest as MIDI and deliver.
 */
 
static int stream_update() {
  uint8_t tmp[1024];
  int tmpc=read(g.srcfd,tmp,sizeof(tmp));
  if (tmpc<=0) return -1;
  /* TODO Lock.
   * stream_update_1() must only be called when we can guarantee that cb_pcm_out() is not running.
   * I would expect that the AudioStream is getting updated on a separate thread, and if that's the case, we're not doing anything to prevent contention.
   * Haven't noticed any trouble yet but unless Raylib is working some kind of kooky magic here, it is definitely going to be a problem eventually.
   */
  int tmpp=0;
  while (tmpp<tmpc) {
    int err=stream_update_1(tmp+tmpp,tmpc-tmpp);
    if (err<=0) {
      // unlock
      return err;
    }
    tmpp+=err;
  }
  // unlock
}

/* Main
 */
 
int main(int argc,char **argv) {
  int err;
  signal(SIGINT,rcvsig);

  /* Read command line.
   */
  g.exename="ray";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if ((arg[0]!='-')||!arg[1]) { // Single dash alone is a srcpath, it means stdin.
      if (g.srcpath) {
        fprintf(stderr,"%s: Multiple input paths ('%s' and '%s').\n",g.exename,g.srcpath,arg);
        return 1;
      }
      g.srcpath=arg;
      continue;
    }
    
    if (arg[1]!='-') { // Short options.
      const char *v=0;
      char k=arg[1];
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=rcvarg(&k,1,v,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error processing option '%c'='%s'\n",g.exename,k,v);
        return 1;
      }
      continue;
    }
    
    if (!arg[2]||(arg[2]=='-')) { // Double dash alone, or more than 2 dashes: reserved.
      fprintf(stderr,"%s: Unexpected argument '%s'\n",g.exename,arg);
      return 1;
    }
    
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=rcvarg(k,kc,v,-1))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing option '%.*s'='%s'\n",g.exename,kc,k,v);
      return 1;
    }
  }
  if (!g.srcpath) {
    fprintf(stderr,"%s: Please specify input path (MIDI file or device).\n",g.exename);
    return 1;
  }
  if (g.rate<1) g.rate=44100;
  else if ((g.rate<200)||(g.rate>200000)) {
    fprintf(stderr,"%s: Unreasonable rate %d hz. Allow 200..200k\n",g.exename,g.rate);
    return 1;
  }
  if (g.chanc<1) g.chanc=2;
  else if (g.chanc>8) {
    fprintf(stderr,"%s: Unreasonable channel count %d. Allow 1..8\n",g.exename,g.chanc);
    return 1;
  }
  // No validation on (buffer). If the driver doesn't like it, it should default.
  
  /* Acquire input file or stream.
   */
  g.srcfd=-1;
  struct stat st={0};
  if (!strcmp(g.srcpath,"-")) {
    g.srcfd=STDIN_FILENO;
    if (fstat(STDIN_FILENO,&st)<0) {
      fprintf(stderr,"%s: Failed to stat stdin: %m\n",g.exename);
      return 1;
    }
  } else {
    if (stat(g.srcpath,&st)<0) {
      fprintf(stderr,"%s: Failed to stat: %m\n",g.exename);
      return 1;
    }
    if ((g.srcfd=open(g.srcpath,O_RDONLY))<0) {
      fprintf(stderr,"%s: Failed to open file: %m\n",g.srcpath);
      return 1;
    }
  }
  if (S_ISCHR(st.st_mode)) {
    // It's a stream, so it's ready to go.
  } else if (S_ISREG(st.st_mode)) {
    // Regular file. Read it all first.
    if ((g.srcc=rdfile(&g.src,g.srcfd))<0) {
      if (g.srcc!=-2) fprintf(stderr,"%s: Failed to read file: %m\n",g.srcpath);
      return 1;
    }
    close(g.srcfd);
  }
  
  /* Prepare driver.
   */
  InitAudioDevice();
  //TODO I don't see anything in Raylib's API to query the driver's rate. I must have missed it; it can't be that they just don't tell us.
  // If we pass zero, it fails.
  // It does log the rate (and it's defaulting to 48k for me).
  g.stream=LoadAudioStream(g.rate,16,g.chanc);
  if (!IsAudioStreamValid(g.stream)) {
    fprintf(stderr,"%s: LoadAudioStream failed\n",g.exename);
    return 1;
  }
  SetAudioStreamCallback(g.stream,cb_pcm_out);
  
  /* Prepare synthesizer.
   */
  if (tfss_init(g.rate,g.chanc)<0) {
    fprintf(stderr,"%s: tfss_init(%d,%d) failed.\n",g.exename,g.rate,g.chanc);
    return 1;
  }
  if (g.src) { // Reading from file. Let tfss do the decoding.
    tfss_play_song(g.src,g.srcc,g.repeat);
  }
  
  /* Run until stopped or depleted.
   */
  if (g.src) {
    fprintf(stderr,"%s: Playing MIDI file '%s' via Raylib at %d hz and %d channels.\n",g.exename,g.srcpath,g.rate,g.chanc);
  } else {
    fprintf(stderr,"%s: Streaming events from '%s' into Raylib at %d hz and %d channels.\n",g.exename,g.srcpath,g.rate,g.chanc);
  }
  PlayAudioStream(g.stream);
  for (;;) {
    if (g.sigc) {
      fprintf(stderr,"%s: Stopping due to signal.\n",g.exename);
      break;
    }
    if (g.src) {
      // Sourced from a file, we can sleep deeply. All the work happens in the I/O thread.
      usleep(100000);
    } else {
      // Sourced from a stream, we must poll it and sleep lightly.
      struct pollfd pollfd={.fd=g.srcfd,.events=POLLIN|POLLERR|POLLHUP};
      err=poll(&pollfd,1,10);
      if (err<0) {
        if (errno==EINTR) break;
        fprintf(stderr,"%s: poll failed: %m\n",g.exename);
        break;
      }
      if (err) {
        if ((err=stream_update())<0) {
          if (err!=-2) fprintf(stderr,"%s: Unspecified error updating stream.\n",g.exename);
          break;
        }
      }
    }
    if (g.clip) {
      fprintf(stderr,"%s: clip\n",g.exename);
      g.clip=0;
    }
  }
  
  /* Clean up and report stats.
   */
  CloseAudioDevice();
  fprintf(stderr,"%s: Normal exit.\n",g.exename);
  //TODO Report CPU consumption.

  return 0;
}

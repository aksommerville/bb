#include "bb_demo.h"
#include "bba/bba.h"
#include "driver/bb_driver.h"
#include <time.h>
#include <signal.h>
#include <unistd.h>

/* Globals.
 */
 
struct bb_driver *demo_driver=0;
struct bba_synth demo_bba={0};

static const struct bb_demo *demo=0;
static double demo_starttime_real,demo_starttime_cpu;
static int64_t demo_framec=0;
static int demo_rate,demo_chanc;
static volatile int demo_sigc=0;

/* Signal handler.
 */
 
static void demo_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++demo_sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Get timestamp.
 * TODO clocks for non-linux platforms
 */
 
double bb_demo_now() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return tv.tv_sec+tv.tv_nsec/1000000000.0;
}
 
double bb_demo_cpu_now() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return tv.tv_sec+tv.tv_nsec/1000000000.0;
}

/* Quit.
 */
 
static void bb_demo_quit(int status) {

  bb_driver_del(demo_driver);
  
  demo->quit();
  
  if (status) {
    fprintf(stderr,"Demo '%s' quitting due to error.\n",demo->name);
  } else if (demo->report_performance) {
    double elapsed_real=bb_demo_now()-demo_starttime_real;
    double elapsed_cpu=bb_demo_cpu_now()-demo_starttime_cpu;
    if (demo_framec>0) {
      fprintf(stderr,
        "%lld frames in %.03f s real. Effective rate %.03f Hz. CPU usage %.06f.\n",
        (long long)demo_framec,elapsed_real,demo_framec/elapsed_real,elapsed_cpu/elapsed_real
      );
    } else {
      fprintf(stderr,
        "Elapsed %.03f s real. CPU usage %.06f.\n",
        elapsed_real,elapsed_cpu/elapsed_real
      );
    }
  }
}

/* PCM callback.
 */
 
static void bb_demo_cb_pcm(int16_t *v,int c,struct bb_driver *driver) {
  demo_framec+=c/driver->chanc;
  
  if (demo_bba.mainrate) {
    if (driver->chanc==2) {
      int framec=c>>1;
      for (;framec-->0;v+=2) {
        v[0]=bba_synth_update(&demo_bba);
        v[1]=v[0];
      }
    } else {
      for (;c-->0;v++) *v=bba_synth_update(&demo_bba);
    }
  
  //TODO other synthesizers
  } else {
    memset(v,0,sizeof(int16_t)*c);
  }
}

/* Init.
 */
 
static int bb_demo_init() {

  signal(SIGINT,demo_rcvsig);

  // Driver may change these.
  // We do too; to force agreement with synth, if we know.
  demo_rate=demo->rate;
  demo_chanc=demo->chanc;
  switch (demo->synth) {
    case 'a': demo_chanc=1; break;
  }
  
  if (demo->driver) {
    if (!(demo_driver=bb_driver_new(0,demo->rate,demo->chanc,BB_SAMPLEFMT_SINT16,(void*)bb_demo_cb_pcm,0))) {
      return -1;
    }
    if (demo_driver->samplefmt!=BB_SAMPLEFMT_SINT16) {
      fprintf(stderr,"Driver '%s' won't accept int16_t samples, and that's all we're doing.\n",demo_driver->type->name);
      return -1;
    }
  }
  
  if ((demo_rate!=demo->rate)||(demo_chanc!=demo->chanc)) {
    fprintf(stderr,"Changed (rate,chanc) (%d,%d) to (%d,%d)\n",demo->rate,demo->chanc,demo_rate,demo_chanc);
  }
  
  switch (demo->synth) {
    case 0: break;
    case 'a': if (bba_synth_init(&demo_bba,demo_rate)<0) return -1; break;
    default: fprintf(stderr,"Demo '%s' request unknown synth '%c'.\n",demo->name,demo->synth); return -1;
  }
  
  fprintf(stderr,"Begin demo '%s'...\n",demo->name);
  
  if (demo->init()<0) return -1;
  
  demo_starttime_real=bb_demo_now();
  demo_starttime_cpu=bb_demo_cpu_now();
  
  return 0;
}

/* Main loop, with context initialized.
 */
 
static int bb_demo_main() {
  while (!demo_sigc) {
    usleep(10000); // maintain about 100 Hz update rate, doesn't need to be perfect
    if (demo_driver) {
      if (bb_driver_update(demo_driver)<0) return -1;
      if (bb_driver_lock(demo_driver)<0) return -1;
    }
    int err=demo->update();
    if (demo_driver) bb_driver_unlock(demo_driver);
    if (err<=0) return err;
  }
  return 0;
}

/* Program main.
 */

int main(int argc,char **argv) {
  
  const char *demoname=BB_DEFAULT_DEMO_NAME;
  if (argc==2) demoname=argv[1];
  else if (argc>2) {
    fprintf(stderr,"%s: Unexpected extra arguments.\n",argv[0]);
    return 1;
  }
  #define _(tag) else if (!strcmp(demoname,#tag)) demo=&bb_demo_metadata_##tag;
  if (0) ;
  BB_FOR_EACH_DEMO
  #undef _
  if (!demo) {
    fprintf(stderr,"Demo '%s' not found. Available demos:\n",demoname);
    #define _(tag) fprintf(stderr,"  %s%s\n",#tag,strcmp(#tag,BB_DEFAULT_DEMO_NAME)?"":" [default]");
    BB_FOR_EACH_DEMO
    #undef _
    return 1;
  }
  
  if ((bb_demo_init()<0)||(bb_demo_main()<0)) {
    bb_demo_quit(1);
    return 1;
  }
  bb_demo_quit(0);
  return 0;
}
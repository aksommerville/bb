#include "bbb_context_internal.h"
#include "share/bb_fs.h"
#include "share/bb_codec.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef O_BINARY
  #define O_BINARY 0
#endif

static int bbb_store_get_cache_path(char *dst,int dsta,struct bbb_store *store,uint32_t sndid);
static struct bbb_pcm *bbb_store_read_cache_file(struct bbb_store *store,const char *path);

/* Cleanup.
 */
 
static void bbb_store_entry_cleanup(struct bbb_store_entry *entry) {
  bbb_pcm_del(entry->pcm);
}
 
void bbb_store_del(struct bbb_store *store) {
  if (!store) return;
  if (store->refc-->1) return;
  
  if (store->configpath) free(store->configpath);
  if (store->cachepath) free(store->cachepath);
  
  bbb_wave_del(store->wave_sine);
  bbb_wave_del(store->wave_losquare);
  bbb_wave_del(store->wave_losaw);
  
  int i=256;
  while (i-->0) bbb_program_del(store->programv[i]);
  
  if (store->entryv) {
    while (store->entryc-->0) {
      bbb_store_entry_cleanup(store->entryv+store->entryc);
    }
    free(store->entryv);
  }
  
  free(store);
}

/* Retain.
 */
 
int bbb_store_ref(struct bbb_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* New.
 */
 
struct bbb_store *bbb_store_new(struct bbb_context *context,const char *configpath,const char *cachepath) {
  struct bbb_store *store=calloc(1,sizeof(struct bbb_store));
  if (!store) return 0;
  
  store->context=context;
  store->refc=1;
  
  store->limit_pcmc=10000; // Very high; I doubt that we want to limit on count of entries.
  store->limit_pcmt=10<<20; // Total sample count. This is the bulk of BBA's memory usage, a useful limit.
  store->target_pcmc=store->limit_pcmc>>1;
  store->target_pcmt=store->limit_pcmt>>1;
  
  if (configpath&&configpath[0]) {
    int c=1; while (configpath[c]) c++;
    if (!(store->configpath=malloc(c+1))) {
      bbb_store_del(store);
      return 0;
    }
    memcpy(store->configpath,configpath,c);
    store->configpath[c]=0;
    store->configpathc=c;
  }
  
  if (cachepath&&cachepath[0]) {
    int c=1; while (cachepath[c]) c++;
    while ((c>1)&&(cachepath[c-1]=='/')) c--; // Must not have a trailing slash
    if (!(store->cachepath=malloc(c+1))) {
      bbb_store_del(store);
      return 0;
    }
    memcpy(store->cachepath,cachepath,c);
    store->cachepath[c]=0;
    store->cachepathc=c;
  }
  
  return store;
}

/* Trivial accessors.
 */
 
int bbb_store_get_pcm_count(const struct bbb_store *store) {
  return store?store->entryc:0;
}
 
int bbb_store_get_print_count(const struct bbb_store *store) {
  return store?store->printc:0;
}

int bbb_store_get_eviction_count(const struct bbb_store *store) {
  return store?store->evictionc:0;
}

int bbb_store_get_memory_estimate(const struct bbb_store *store) {
  return store?(store->pcmtotal<<1):0;
}

int bbb_store_set_pcm_count_limit(struct bbb_store *store,int pcmc) {
  if (!store) return -1;
  if (pcmc>1) {
    store->limit_pcmc=pcmc;
    store->target_pcmc=pcmc>>1;
  }
  return store->limit_pcmc;
}

int bbb_store_set_memory_limit(struct bbb_store *store,int bytec) {
  if (!store) return -1;
  if (bytec>1) {
    store->limit_pcmt=bytec;
    store->target_pcmt=bytec>>1;
  }
  return store->limit_pcmt;
}

/* Load.
 */
 
int bbb_store_load(struct bbb_store *store) {
  
  // If config path unset, load the defaults.
  if (!store->configpathc) return bbb_store_load_default(store);
  
  // Read fails, eg doesn't exist, default it but issue a warning.
  uint8_t *src=0;
  int srcc=bb_file_read(&src,store->configpath);
  if (srcc<0) {
    fprintf(stderr,"%s:WARNING: Failed to read file. Generating default instruments.\n",store->configpath);
    return bbb_store_load_default(store);
  }
  
  // Verify signature.
  if ((srcc<4)||memcmp(src,"\x00\xbb\xbaR",4)) {
    free(src);
    return -1;
  }
  int srcp=4;
  
  // Read programs if the next byte is zero.
  if ((srcp<srcc)&&!src[srcp]) {
    int pid=-1;
    while (srcp<srcc) {
    
      // End of program list, if the next pid is out of order.
      if (src[srcp]<=pid) break;
      pid=src[srcp++];
      
      // Measure program.
      const void *program=src+srcp;
      int programc=bbb_measure_program(program,srcc-srcp);
      if (programc<0) {
        free(src);
        return -1;
      }
      srcp+=programc;
      
      // Install it.
      if (bbb_store_set_program(store,pid,program,programc)<0) {
        free(src);
        return -1;
      }
    }
  }
  
  // Further content may be defined in the future...
  
  free(src);
  return 0;
}

/* Install program.
 */
 
int bbb_store_set_program(struct bbb_store *store,uint8_t pid,const void *src,int srcc) {
  struct bb_decoder decoder={.src=src,.srcc=srcc};
  struct bbb_program *program=bbb_program_new(store->context,&decoder);
  if (!program) return -1;
  bbb_program_del(store->programv[pid]);
  store->programv[pid]=program;
  return 0;
}

/* Get PCM.
 */
 
struct bbb_pcm *bbb_store_get_pcm(
  struct bbb_printer **printerrtn,
  struct bbb_store *store,
  uint32_t sndid
) {

  // Already have it? Great!
  int p=bbb_store_search(store,sndid);
  if (p>=0) {
    if (printerrtn) *printerrtn=0;
    struct bbb_pcm *pcm=store->entryv[p].pcm;
    if (bbb_pcm_ref(pcm)<0) return 0;
    store->entryv[p].access=store->access_next++;
    return pcm;
  }
  p=-p-1;
  
  // Can we fetch it from the disk cache?
  // Anything goes wrong, let it pass through to printing.
  if (store->cachepathc) {
    char path[1024];
    int pathc=bbb_store_get_cache_path(path,sizeof(path),store,sndid);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      struct bbb_pcm *pcm=bbb_store_read_cache_file(store,path);
      if (pcm) {
        pcm->sndid=sndid;
        bbb_store_insert(store,p,sndid,pcm);
        //fprintf(stderr,"%s: Fetched sound 0x%08x from cache.\n",path,sndid);
        return pcm; // handoff
      }
    }
  }
  
  // Begin printing.
  uint8_t pid=sndid>>16;
  struct bbb_program *program=store->programv[pid];
  if (!program) return 0;
  uint8_t noteid=sndid>>8,velocity=sndid;
  struct bbb_printer *printer=bbb_print(program,noteid,velocity);
  //fprintf(stderr,"%s:%d %02x %02x %02x printer=%p\n",__FILE__,__LINE__,pid,noteid,velocity,printer);
  if (!printer) return 0;
  store->printc++;
  
  // Add to the PCM list.
  if (bbb_store_insert(store,p,sndid,printer->pcm)<0) {
    bbb_printer_del(printer);
    return 0;
  }
  
  // If the caller did not provide a printer return vector, print the whole thing synchronously.
  if (!printerrtn) {
    if (bbb_printer_update(printer,printer->pcm->c)<0) {
      bbb_printer_del(printer);
      return 0;
    }
    struct bbb_pcm *pcm=printer->pcm;
    if (bbb_pcm_ref(pcm)<0) {
      bbb_printer_del(printer);
      return 0;
    }
    bbb_printer_del(printer);
    return pcm;
  }
  
  // Return both objects, let the caller print it over time.
  if (bbb_pcm_ref(printer->pcm)<0) {
    bbb_printer_del(printer);
    return 0;
  }
  *printerrtn=printer;
  return printer->pcm;
}

/* Check the PCM cache and evict members if too big.
 * Call this after adding or replacing anything.
 */
 
static int bbb_store_cmp_access(const void *a,const void *b) {
  const struct bbb_store_entry *A=a,*B=b;
  return A->access-B->access;
}
 
static int bbb_store_cmp_sndid(const void *a,const void *b) {
  const struct bbb_store_entry *A=a,*B=b;
  return A->sndid-B->sndid;
}
 
static void bbb_store_gc_pcm(struct bbb_store *store) {

  // If entry count and total size are both within limits, do nothing.
  if ((store->entryc<=store->limit_pcmc)&&(store->pcmtotal<=store->limit_pcmt)) return;
  
  // Reset access counters so they are in ascending order.
  // If we left them untouched, the sequence could be interrupted once.
  struct bbb_store_entry *entry=store->entryv;
  int i=store->entryc;
  for (;i-->0;entry++) entry->access=store->access_next-entry->access;
  
  // Sort entries by access order backward: Most recently-accessed PCM at the front.
  qsort(store->entryv,store->entryc,sizeof(struct bbb_store_entry),bbb_store_cmp_access);
  
  // Drop PCMs from the tail until both targets are met.
  int rmc=0;
  while (store->entryc>0) {
    if ((store->entryc<=store->target_pcmc)&&(store->pcmtotal<=store->target_pcmt)) break;
    rmc++;
    store->entryc--;
    entry=store->entryv+store->entryc;
    store->pcmtotal-=entry->pcm->c;
    bbb_store_entry_cleanup(entry);
  }
  
  // Reset access.
  store->access_next=0;
  for (i=store->entryc,entry=store->entryv;i-->0;entry++) {
    entry->access=store->access_next++;
  }
  
  // Restore natural order (by sndid).
  qsort(store->entryv,store->entryc,sizeof(struct bbb_store_entry),bbb_store_cmp_sndid);
  
  //fprintf(stderr,"*** bbb evicted %d PCM entries. now count=%d total=%d\n",rmc,store->entryc,store->pcmtotal);
  store->evictionc++;
}

/* PCM list.
 */
 
int bbb_store_search(const struct bbb_store *store,uint32_t sndid) {
  int lo=0,hi=store->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (sndid<store->entryv[ck].sndid) hi=ck;
    else if (sndid>store->entryv[ck].sndid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int bbb_store_insert(struct bbb_store *store,int p,uint32_t sndid,struct bbb_pcm *pcm) {
  if ((p<0)||(p>store->entryc)) return -1;
  if (p&&(sndid<=store->entryv[p-1].sndid)) return -1;
  if ((p<store->entryc)&&(sndid>=store->entryv[p].sndid)) return -1;
  
  if (store->entryc>=store->entrya) {
    int na=store->entrya+32;
    if (na>INT_MAX/sizeof(struct bbb_store_entry)) return -1;
    void *nv=realloc(store->entryv,sizeof(struct bbb_store_entry)*na);
    if (!nv) return -1;
    store->entryv=nv;
    store->entrya=na;
  }
  
  if (bbb_pcm_ref(pcm)<0) return -1;
  
  struct bbb_store_entry *entry=store->entryv+p;
  memmove(entry+1,entry,sizeof(struct bbb_store_entry)*(store->entryc-p));
  store->entryc++;
  
  entry->sndid=sndid;
  entry->pcm=pcm;
  entry->access=store->access_next++;
  store->pcmtotal+=pcm->c;
  pcm->sndid=sndid;
  
  bbb_store_gc_pcm(store);
  
  return 0;
}

int bbb_store_replace(struct bbb_store *store,int p,struct bbb_pcm *pcm) {
  if ((p<0)||(p>=store->entryc)) return -1;
  struct bbb_store_entry *entry=store->entryv+p;
  if (bbb_pcm_ref(pcm)<0) return -1;
  store->pcmtotal-=entry->pcm->c;
  store->pcmtotal+=pcm->c;
  bbb_pcm_del(entry->pcm);
  entry->pcm=pcm;
  entry->access=store->access_next++;
  bbb_store_gc_pcm(store);
  return 0;
}

/* Generate path to a cache file for a given sndid.
 */
 
static int bbb_store_get_cache_path(char *dst,int dsta,struct bbb_store *store,uint32_t sndid) {
  if (dsta<0) return -1;
  if (!sndid||(sndid&0xff000000)) return -1;
  if (!store->cachepathc) return -1;
  int dstc=snprintf(dst,dsta,
    "%.*s/%d/%03d/%04x",
    store->cachepathc,store->cachepath,
    bbb_context_get_rate(store->context),
    (sndid>>16)&0xff,sndid&0xffff
  );
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* Open cache file for writing.
 * Creates the allowed intermediate directories if needed.
 */
 
static int bbb_store_cache_openw(struct bbb_store *store,const char *path) {

  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  if (fd>=0) return fd;
  if (errno!=ENOENT) return -1;
  
  // Try to create parent directories...
  char dirpath[1024];
  int pathc=0; while (path[pathc]) pathc++;
  if (pathc>=sizeof(dirpath)) return -1;
  memcpy(dirpath,path,pathc+1);
  int dirpathp=store->cachepathc;
  while (dirpath[dirpathp]=='/') {
    dirpath[dirpathp]=0;
    if (mkdir(dirpath,0775)<0) {
      if (errno!=EEXIST) return -1;
    }
    dirpath[dirpathp]='/';
    dirpathp++;
    while (1) {
      if (!dirpath[dirpathp]) break;
      if (dirpath[dirpathp]=='/') break;
      dirpathp++;
    }
  }
  
  // ...and try opening again.
  return open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
}

/* Write PCM cache file.
 */
 
static int bbb_store_write_cache_file(struct bbb_store *store,int fd,const struct bbb_pcm *pcm) {
  uint8_t hdr[4]={
    pcm->loopa>>8,pcm->loopa,
    pcm->loopz>>8,pcm->loopz,
  };
  if (write(fd,hdr,sizeof(hdr))!=sizeof(hdr)) return -1;
  int wrc=pcm->c<<1;
  if (write(fd,pcm->v,wrc)!=wrc) return -1;
  return 0;
}

/* Read file from the cache.
 */
 
static struct bbb_pcm *bbb_store_read_cache_file(struct bbb_store *store,const char *path) {
  /* If we wanted to get really efficient about it, we could seek for the file's length,
   * allocate the PCM object, then read directly into it.
   * That I feel is more effort than it's worth, just makes a new opportunity to fuck something up.
   */
  uint8_t *src=0;
  int srcc=bb_file_read(&src,path);
  if (srcc<0) return 0;
  
  // Minimum length 6: 4-byte header plus at least one sample.
  if (srcc<6) {
    free(src);
    return 0;
  }
  int samplec=(srcc-4)>>1;
  struct bbb_pcm *pcm=bbb_pcm_new(samplec);
  if (!pcm) {
    free(src);
    return 0;
  }
  
  pcm->loopa=(src[0]<<8)|src[1];
  pcm->loopz=(src[2]<<8)|src[3];
  memcpy(pcm->v,src+4,samplec<<1);
  free(src);
  
  if ((pcm->loopa>pcm->loopz)||(pcm->loopz>pcm->c)) {
    fprintf(stderr,"%s:WARNING: Invalid loop %d..%d (c=%d).\n",path,pcm->loopa,pcm->loopz,pcm->c);
    pcm->loopa=0;
    pcm->loopz=0;
  }
  
  return pcm;
}

/* Print finished: Consider persisting to disk cache.
 */
 
int bbb_store_print_finished(struct bbb_store *store,struct bbb_pcm *pcm) {

  // Get out quick if we don't do disk cache.
  if (!store||!pcm) return -1;
  if (!store->cachepathc) return 0;
  
  // Loop points get stored in 16 bits.
  // Get out if that's not possible (should usually be possible, 64k is a long time).
  if ((pcm->loopa&0xffff0000)||(pcm->loopz&0xffff0000)) return 0;
  
  // Is this PCM one of ours? 
  if (!pcm->sndid) return 0;
  char path[1024];
  int pathc=bbb_store_get_cache_path(path,sizeof(path),store,pcm->sndid);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  // Write it.
  int fd=bbb_store_cache_openw(store,path);
  if (fd<0) return -1;
  if (bbb_store_write_cache_file(store,fd,pcm)<0) {
    unlink(path);
    return -1;
  }
  close(fd);
  
  //fprintf(stderr,"%s: Saved PCM to disk cache.\n",path);
  
  return 0;
}

/* Ad-hoc wave generator and cache.
 */
 
struct bbb_wave *bbb_wave_get_sine(struct bbb_context *context) {
  struct bbb_store *store=context?context->store:0;
  if (!store) return 0;
  if (!store->wave_sine) {
    if (!(store->wave_sine=bbb_wave_new())) return 0;
    bbb_wave_generate_sine(store->wave_sine->v,BBB_WAVE_SIZE);
  }
  return store->wave_sine;
}

struct bbb_wave *bbb_wave_get_losquare(struct bbb_context *context) {
  struct bbb_store *store=context?context->store:0;
  if (!store) return 0;
  if (!store->wave_losquare) {
    if (!(store->wave_losquare=bbb_wave_new())) return 0;
    bbb_wave_generate_losquare(store->wave_losquare->v,BBB_WAVE_SIZE,32000,0.15);
  }
  return store->wave_losquare;
}

struct bbb_wave *bbb_wave_get_losaw(struct bbb_context *context) {
  struct bbb_store *store=context?context->store:0;
  if (!store) return 0;
  if (!store->wave_losaw) {
    if (!(store->wave_losaw=bbb_wave_new())) return 0;
    bbb_wave_generate_losaw(store->wave_losaw->v,BBB_WAVE_SIZE,150.0);
  }
  return store->wave_losaw;
}

/* Convenience wave generator.
 */

struct bbb_wave *bbb_wave_from_shape(struct bbb_context *context,uint8_t shape) {
  struct bbb_store *store=context?context->store:0;
  if (!store) return 0;
  switch (shape) {
    case BBB_SHAPE_SINE: {
        struct bbb_wave *wave=bbb_wave_get_sine(context);
        if (bbb_wave_ref(wave)<0) return 0;
        return wave;
      }
    case BBB_SHAPE_SQUARE: {
        struct bbb_wave *wave=bbb_wave_new();
        if (!wave) return 0;
        bbb_wave_generate_square(wave->v,BBB_WAVE_SIZE);
        return wave;
      }
    case BBB_SHAPE_SAW: {
        struct bbb_wave *wave=bbb_wave_new();
        if (!wave) return 0;
        bbb_wave_generate_saw(wave->v,BBB_WAVE_SIZE,-32767,32767);
        return wave;
      }
    case BBB_SHAPE_TRIANGLE: {
        struct bbb_wave *wave=bbb_wave_new();
        if (!wave) return 0;
        bbb_wave_generate_triangle(wave->v,BBB_WAVE_SIZE);
        return wave;
      }
    case BBB_SHAPE_LOSQUARE: {
        struct bbb_wave *wave=bbb_wave_get_losquare(context);
        if (bbb_wave_ref(wave)<0) return 0;
        return wave;
      }
    case BBB_SHAPE_LOSAW: {
        struct bbb_wave *wave=bbb_wave_get_losaw(context);
        if (bbb_wave_ref(wave)<0) return 0;
        return wave;
      }
    case BBB_SHAPE_NOISE: {
        struct bbb_wave *wave=bbb_wave_new();
        if (!wave) return 0;
        int16_t *v=wave->v;
        int i=BBB_WAVE_SIZE;
        for (;i-->0;v++) *v=rand();
        return wave;
      }
  }
  return 0;
}

#include "bbb_context_internal.h"
#include "share/bb_fs.h"
#include "share/bb_codec.h"

//TODO cache size limit
//TODO persist cache to fs

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

/* Load.
 */
 
int bbb_store_load(struct bbb_store *store) {
  
  // No config path is equivalent to an empty file.
  if (!store->configpathc) return 0;
  
  // No error if read fails, presumably the file doesn't exist.
  uint8_t *src=0;
  int srcc=bb_file_read(&src,store->configpath);
  if (srcc<0) return 0;
  
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
    return pcm;
  }
  p=-p-1;
  
  // Begin printing.
  uint8_t pid=sndid>>16;
  struct bbb_program *program=store->programv[pid];
  if (!program) return 0;
  uint8_t noteid=sndid>>8,velocity=sndid;
  struct bbb_printer *printer=bbb_print(program,noteid,velocity);
  if (!printer) return 0;
  
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
  
  return 0;
}

int bbb_store_replace(struct bbb_store *store,int p,struct bbb_pcm *pcm) {
  if ((p<0)||(p>=store->entryc)) return -1;
  struct bbb_store_entry *entry=store->entryv+p;
  if (bbb_pcm_ref(pcm)<0) return -1;
  bbb_pcm_del(entry->pcm);
  entry->pcm=pcm;
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

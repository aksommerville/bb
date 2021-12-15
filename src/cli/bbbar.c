#include "bb_cli.h"
#include "bbb/bbb.h"
#include "share/bb_fs.h"
#include "share/bb_codec.h"
#include "share/bb_serial.h"

/* -c: context
 */
 
struct barc_context {
  struct bb_encoder dst;
  const char *path; // src or dst, whatever we're working on right now
  struct bb_encoder programv[256]; // borrowing encoder just for cleanup, really
};

static void barc_context_cleanup(struct barc_context *context) {
  bb_encoder_cleanup(&context->dst);
  int i=0; for (;i<256;i++) bb_encoder_cleanup(context->programv+i);
}

/* -c: Read pid from file name.
 * <0 if invalid
 * 0..255 = real pid
 * >255 if archive
 */
 
static int barc_get_current_pid(const struct barc_context *context) {
  if (!context||!context->path) return -1;
  
  int pid=0,basec=0,p=0;
  int pidstatus=0; // (-1,0,1,2)=(invalid,empty,incomplete,valid)
  const char *base=context->path;
  for (;context->path[p];p++) {
    if (context->path[p]=='/') {
      pid=0;
      basec=0;
      base=context->path+p+1;
      pidstatus=0;
    } else if (!pidstatus) {
      int digit=context->path[p]-'0';
      if ((digit<0)||(digit>9)) {
        pidstatus=-1;
      } else {
        pidstatus=1;
        pid=digit;
      }
      basec++;
    } else if (pidstatus==1) {
      int digit=context->path[p]-'0';
      if ((digit<0)||(digit>9)) {
        pidstatus=2;
      } else {
        pid*=10;
        pid+=digit;
        if (pid>255) pidstatus=-1;
      }
      basec++;
    } else {
      basec++;
    }
  }
  
  if ((basec>=6)&&!memcmp(base,".bbbar",6)) return 256;
  if (pidstatus<1) return -1;
  return pid;
}

/* -c: Add binary program.
 */
 
static int barc_add_program_binary(struct barc_context *context,int pid,const void *src,int srcc) {

  // 0 is special; leave whatever was in place there
  if ((srcc>=1)&&(((uint8_t*)src)[0]==0x00)) return 0;
  
  // Measure it again: If the length doesn't match, use the shorter and issue a warning.
  int realc=bbb_measure_program(src,srcc);
  if ((realc<0)||(realc>srcc)) {
    fprintf(stderr,"%s:ERROR: Program 0x%02x invalid length.\n",context->path,pid);
    return -1;
  } else if (realc<srcc) {
    fprintf(stderr,"%s:WARNING: Program 0x%02x input size %d, but measured %d.\n",context->path,pid,srcc,realc);
    srcc=realc;
  }
  
  struct bb_encoder *dst=context->programv+pid;
  dst->c=0;
  if (bb_encode_raw(dst,src,srcc)<0) return -1;
  
  return 0;
}

/* -c: Add text program.
 * The text format is just a hex dump with comments.
 */
 
static int barc_add_program_text(struct barc_context *context,int pid,const char *src,int srcc) {

  // Decode directly into the stored binary.
  struct bb_encoder *dst=context->programv+pid;
  dst->c=0;
  
  // Unhexdump...
  struct bb_decoder decoder={.src=src,.srcc=srcc};
  int lineno=0,linec;
  const char *line;
  while ((linec=bb_decode_line(&line,&decoder))>0) {
    lineno++;
    int hi=-1; // 0..15 midbyte, or <0 between bytes
    for (;linec-->0;line++) {
      if (*line=='#') break;
      if ((unsigned char)(*line)<=0x20) continue;
      int digit=bb_hexdigit_eval(*line);
      if (digit<0) {
        fprintf(stderr,"%s:%d: Unexpected character '%c'\n",context->path,lineno,*line);
        return -1;
      }
      if (hi>=0) {
        if (bb_encode_intbe(dst,(hi<<4)|digit,1)<0) return -1;
        hi=-1;
      } else {
        hi=digit;
      }
    }
    if (hi>=0) {
      fprintf(stderr,"%s:%d: Uneven count of hex digits on line.\n",context->path,lineno);
      return -1;
    }
  }
  
  // We're supposed to treat type zero as "don't change", but ugh... complicated, not worth it.
  if ((dst->c>=1)&&!((uint8_t*)(dst->v))[0]) {
    fprintf(stderr,
      "%s:WARNING: Program type zero, we should have ignored this, but that's hard for the text processor to do.\n",
      context->path
    );
  }
  
  // Important! Re-measure what we produced and ensure that it matches.
  // It is very easy in this format to add unexpected trailing data, and that would break the decoder.
  int realc=bbb_measure_program(dst->v,dst->c);
  if ((realc<0)||(realc>dst->c)) {
    fprintf(stderr,"%s:ERROR: Program 0x%02x invalid length.\n",context->path,pid);
    return -1;
  } else if (realc<dst->c) {
    fprintf(stderr,
      "%s:WARNING: Program 0x%02x input size %d, but measured %d. Ignoring the last %d bytes.\n",
      context->path,pid,dst->c,realc,dst->c-realc
    );
    dst->c=realc;
  }
  
  return 0;
}

/* -c: Add another archive.
 */
 
static int barc_add_subarchive(struct barc_context *context,const uint8_t *src,int srcc) {
  
  if ((srcc<4)||memcmp(src,"\x00\xbb\xbaR",4)) {
    fprintf(stderr,"%s: bbbar signature mismatch\n",context->path);
    return -1;
  }
  
  // No content: OK we're done.
  // First byte after signature nonzero: No programs (what actually is there, not yet defined). But safe to ignore.
  if ((srcc<5)||src[4]) return 0;
  
  int pvpid=-1;
  int srcp=4;
  while (srcp<srcc) {
  
    // Programs list ends when the leading byte is less than the expected pid.
    if (src[srcp]<=pvpid) break;
    pvpid=src[srcp++];
    
    int len=bbb_measure_program(src+srcp,srcc-srcp);
    if (len<0) {
      fprintf(stderr,"%s:%d/%d: Error measuring program 0x%02x\n",context->path,srcp,srcc,pvpid);
      return -1;
    }
    
    if (barc_add_program_binary(context,pvpid,src+srcp,len)<0) return -1;
    srcp+=len;
  }
  
  return 0;
}

/* -c: Add regular file, already read.
 * This only decides what kind of file, and dispatches.
 */
 
static int barc_add_file(struct barc_context *context,const uint8_t *src,int srcc) {
  int pid=barc_get_current_pid(context);
  if (pid<0) {
    fprintf(stderr,"%s:WARNING: Unable to determine pid from file name, ignoring file.\n",context->path);
    return 0;
  }
  if (pid>0xff) return barc_add_subarchive(context,src,srcc);
  if (srcc<1) return barc_add_program_binary(context,pid,src,srcc);
  if ((src[0]==0x09)||(src[0]==0x0a)||(src[0]==0x0d)) return barc_add_program_text(context,pid,(char*)src,srcc);
  if (src[0]<0x20) return barc_add_program_binary(context,pid,src,srcc);
  if (src[0]<0x7f) return barc_add_program_text(context,pid,(char*)src,srcc);
  return barc_add_program_binary(context,pid,src,srcc);
}

/* -c: Directory callback.
 */
 
static int barc_add_source(struct barc_context *context,const char *path,char type);
 
static int barc_cb_dir(const char *path,const char *base,char type,void *userdata) {
  struct barc_context *context=userdata;
  const char *pvpath=context->path;
  int err=barc_add_source(context,path,type);
  context->path=pvpath;
  if (err<0) return -1;
  return 0;
}

/* -c: Add a file or directory.
 */
 
static int barc_add_source(struct barc_context *context,const char *path,char type) {
  context->path=path;
  if (!type) type=bb_file_get_type(path);
  
  if (type=='f') {
    void *src=0;
    int srcc=bb_file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Failed to read file.\n",path);
      return -1;
    }
    int err=barc_add_file(context,src,srcc);
    free(src);
    return err;
  }
  
  if (type=='d') {
    return bb_dir_read(path,barc_cb_dir,context);
  }
  
  fprintf(stderr,"%s:WARNING: Unexpected file type '%c', ignoring file.\n",path,type);
  return 0;
}

/* -c: Digest resources, encode, write file.
 */
 
static int barc_finish(struct barc_context *context) {

  // Program zero must exist. Add a dummy if we don't have it.
  if (!context->programv[0].c) {
    if (bb_encode_intbe(context->programv,BBB_PROGRAM_TYPE_dummy,1)<0) return -1;
  }

  context->dst.c=0;
  if (bb_encode_raw(&context->dst,"\x00\xbb\xbaR",4)<0) return -1;
  
  int pid=0;
  const struct bb_encoder *src=context->programv;
  for (;pid<256;pid++,src++) {
    if (src->c) {
      if (bb_encode_intbe(&context->dst,pid,1)<0) return -1;
      if (bb_encode_raw(&context->dst,src->v,src->c)<0) return -1;
    }
  }

  if (bb_file_write(context->path,context->dst.v,context->dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file.\n",context->path);
    return -1;
  }
  return 0;
}

/* -c: Main.
 */
 
static int bbbar_c(const char *dstpath,char **srcpathv,int srcpathc) {
  struct barc_context context={};
  for (;srcpathc-->0;srcpathv++) {
    if (barc_add_source(&context,*srcpathv,0)<0) return 1;
  }
  context.path=dstpath;
  int err=barc_finish(&context);
  barc_context_cleanup(&context);
  return (err<0)?1:0;
}

/* -x
 */
 
static int bbbar_x(const char *path) {
  fprintf(stderr,"TODO %s %s\n",__func__,path);
  return 1;
}

/* -t
 */
 
static int bbbar_t(const char *path) {
  fprintf(stderr,"TODO %s %s\n",__func__,path);
  return 1;
}

/* Main.
 */
 
int main_bbbar(char **argv,int argc) {

  if (
    (argc<2)||
    (argv[0][0]!='-')||!argv[0][1]||argv[0][2]||
    (argv[1][0]=='-')
  ) {
   _bad_usage_:;
    fprintf(stderr,"Usage: beepbot bbbar -c|-x|-t ARCHIVE [INPUTS...]\n");
    return 1;
  }
  char command=argv[0][1];
  const char *archivepath=argv[1];
  
  switch (command) {
    case 'c': return bbbar_c(archivepath,argv+2,argc-2);
    case 'x': {
        if (argc>2) goto _bad_usage_;
        return bbbar_x(archivepath);
      }
    case 't': {
        if (argc>2) goto _bad_usage_;
        return bbbar_t(archivepath);
      }
    default: goto _bad_usage_;
  } 
}

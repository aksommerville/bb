#include "bb_codec.h"
#include "bb_serial.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

/* Cleanup.
 */

void bb_encoder_cleanup(struct bb_encoder *encoder) {
  if (encoder->v) free(encoder->v);
  memset(encoder,0,sizeof(struct bb_encoder));
}

/* Grow buffer.
 */
 
int bb_encoder_require(struct bb_encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->c>INT_MAX-addc) return -1;
  int na=encoder->c+addc;
  if (na<INT_MAX-256) na=(na+256)&~255;
  void *nv=realloc(encoder->v,na);
  if (!nv) return -1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

/* Replace content.
 */

int bb_encoder_replace(struct bb_encoder *encoder,int p,int c,const void *src,int srcc) {
  if (p<0) p=encoder->c;
  if (c<0) c=encoder->c-p;
  if ((p<0)||(c<0)||(p>encoder->c-c)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (srcc!=c) {
    if (bb_encoder_require(encoder,srcc-c)<0) return -1;
    memmove(encoder->v+p+srcc,encoder->v+p+c,encoder->c-c-p);
    encoder->c+=srcc-c;
  }
  memcpy(encoder->v+p,src,srcc);
  return 0;
}

/* Append chunks with optional length.
 */
 
int bb_encode_null(struct bb_encoder *encoder,int c) {
  if (c<1) return 0;
  if (bb_encoder_require(encoder,c)<0) return -1;
  memset(encoder->v+encoder->c,0,c);
  encoder->c+=c;
  return 0;
}

int bb_encode_raw(struct bb_encoder *encoder,const void *src,int srcc) {
  return bb_encoder_replace(encoder,encoder->c,0,src,srcc);
}

int bb_encode_intlelen(struct bb_encoder *encoder,const void *src,int srcc,int size) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (bb_encode_intle(encoder,srcc,size)<0) return -1;
  if (bb_encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int bb_encode_intbelen(struct bb_encoder *encoder,const void *src,int srcc,int size) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (bb_encode_intbe(encoder,srcc,size)<0) return -1;
  if (bb_encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int bb_encode_vlqlen(struct bb_encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (bb_encode_vlq(encoder,srcc)<0) return -1;
  if (bb_encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

int bb_encode_vlq5len(struct bb_encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (bb_encode_vlq5(encoder,srcc)<0) return -1;
  if (bb_encoder_replace(encoder,encoder->c,0,src,srcc)<0) return -1;
  return 0;
}

/* Insert length in the past.
 */

int bb_encoder_insert_intlelen(struct bb_encoder *encoder,int p,int size) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=bb_intle_encode(tmp,sizeof(tmp),encoder->c-p,size);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return bb_encoder_replace(encoder,p,0,tmp,tmpc);
}

int bb_encoder_insert_intbelen(struct bb_encoder *encoder,int p,int size) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=bb_intbe_encode(tmp,sizeof(tmp),encoder->c-p,size);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return bb_encoder_replace(encoder,p,0,tmp,tmpc);
}

int bb_encoder_insert_vlqlen(struct bb_encoder *encoder,int p) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=bb_vlq_encode(tmp,sizeof(tmp),encoder->c-p);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return bb_encoder_replace(encoder,p,0,tmp,tmpc);
}

int bb_encoder_insert_vlq5len(struct bb_encoder *encoder,int p) {
  if ((p<0)||(p>encoder->c)) return -1;
  char tmp[5];
  int tmpc=bb_vlq_encode(tmp,sizeof(tmp),encoder->c-p);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  return bb_encoder_replace(encoder,p,0,tmp,tmpc);
}

/* Append formatted string.
 */

int bb_encode_fmt(struct bb_encoder *encoder,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err==INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) { // sic < not <=
      encoder->c+=err;
      return 0;
    }
    if (bb_encoder_require(encoder,err+1)<0) return -1;
  }
}

/* Append scalars.
 */

int bb_encode_intle(struct bb_encoder *encoder,int src,int size) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_intle_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_intbe(struct bb_encoder *encoder,int src,int size) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_intbe_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_vlq(struct bb_encoder *encoder,int src) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_vlq_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_vlq5(struct bb_encoder *encoder,int src) {
  if (bb_encoder_require(encoder,5)<0) return -1;
  int err=bb_vlq5_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_utf8(struct bb_encoder *encoder,int src) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_utf8_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_fixed(struct bb_encoder *encoder,double src,int size,int fractbitc) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_fixed_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size,fractbitc);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

int bb_encode_fixedf(struct bb_encoder *encoder,float src,int size,int fractbitc) {
  if (bb_encoder_require(encoder,4)<0) return -1;
  int err=bb_fixedf_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size,fractbitc);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

/* Append conversions.
 */
 
int bb_encode_base64(struct bb_encoder *encoder,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  while (1) {
    int err=bb_base64_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (bb_encoder_require(encoder,err)<0) return -1;
  }
  return 0;
}

/* Append a JSON string, regardless of context.
 */
 
int bb_encode_json_string_token(struct bb_encoder *encoder,const char *src,int srcc) {
  while (1) {
    int err=bb_string_repr(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (bb_encoder_require(encoder,err)<0) return -1;
  }
}

/* Append a comma if we are inside a structure, and the last non-space character was not the opener.
 */
 
static int bb_encode_json_comma_if_needed(struct bb_encoder *encoder) {
  if ((encoder->jsonctx!='{')&&(encoder->jsonctx!='[')) return 0;
  int insp=encoder->c;
  while ((insp>0)&&((unsigned char)encoder->v[insp-1]<=0x20)) insp--;
  if (!insp) return -1; // No opener even? Something is fubar'd.
  if (encoder->v[insp-1]==encoder->jsonctx) return 0; // emitting first member, no comma
  return bb_encoder_replace(encoder,insp,0,",",1);
}

/* Begin JSON expression. Check for errors, emit key, etc.
 */
 
static int bb_encode_json_prepare(struct bb_encoder *encoder,const char *k,int kc) {
  if (encoder->jsonctx<0) return -1;
  if (encoder->jsonctx=='{') {
    if (!k) return encoder->jsonctx=-1;
    if (bb_encode_json_comma_if_needed(encoder)<0) return encoder->jsonctx=-1;
    if (bb_encode_json_string_token(encoder,k,kc)<0) return encoder->jsonctx=-1;
    if (bb_encode_raw(encoder,":",1)<0) return encoder->jsonctx=-1;
    return 0;
  }
  if (k) return encoder->jsonctx=-1;
  if (encoder->jsonctx=='[') {
    if (bb_encode_json_comma_if_needed(encoder)<0) return encoder->jsonctx=-1;
  }
  return 0;
}

/* Start JSON structure.
 */

int bb_encode_json_object_start(struct bb_encoder *encoder,const char *k,int kc) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  if (bb_encode_raw(encoder,"{",1)<0) return encoder->jsonctx=-1;
  int jsonctx=encoder->jsonctx;
  encoder->jsonctx='{';
  return encoder->jsonctx;
}

int bb_encode_json_array_start(struct bb_encoder *encoder,const char *k,int kc) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  if (bb_encode_raw(encoder,"[",1)<0) return encoder->jsonctx=-1;
  int jsonctx=encoder->jsonctx;
  encoder->jsonctx='[';
  return encoder->jsonctx;
}

/* End JSON structure.
 */
 
int bb_encode_json_object_end(struct bb_encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='{') return encoder->jsonctx=-1;
  if (bb_encode_raw(encoder,"}",1)<0) return encoder->jsonctx=-1;
  encoder->jsonctx=jsonctx;
  return 0;
}

int bb_encode_json_array_end(struct bb_encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='[') return encoder->jsonctx=-1;
  if (bb_encode_raw(encoder,"]",1)<0) return encoder->jsonctx=-1;
  encoder->jsonctx=jsonctx;
  return 0;
}

/* JSON primitives.
 */

int bb_encode_json_preencoded(struct bb_encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  if (bb_encode_raw(encoder,v,vc)<0) return encoder->jsonctx=-1;
  return 0;
}

int bb_encode_json_null(struct bb_encoder *encoder,const char *k,int kc) {
  return bb_encode_json_preencoded(encoder,k,kc,"null",4);
}

int bb_encode_json_boolean(struct bb_encoder *encoder,const char *k,int kc,int v) {
  return bb_encode_json_preencoded(encoder,k,kc,v?"true":"false",-1);
}

int bb_encode_json_int(struct bb_encoder *encoder,const char *k,int kc,int v) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  while (1) {
    int err=bb_decsint_repr(encoder->v+encoder->c,encoder->a-encoder->c,v);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (bb_encoder_require(encoder,err)<0) return -1;
  }
}

int bb_encode_json_float(struct bb_encoder *encoder,const char *k,int kc,double v) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  while (1) {
    int err=bb_float_repr(encoder->v+encoder->c,encoder->a-encoder->c,v);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (bb_encoder_require(encoder,err)<0) return -1;
  }
}

int bb_encode_json_string(struct bb_encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (bb_encode_json_prepare(encoder,k,kc)<0) return -1;
  if (bb_encode_json_string_token(encoder,v,vc)<0) return encoder->jsonctx=-1;
  return 0;
}

/* Assert JSON complete.
 */

int bb_encode_json_done(const struct bb_encoder *encoder) {
  if (encoder->jsonctx) return -1;
  return 0;
}

#include "bb_codec.h"
#include "bb_serial.h"
#include <string.h>

/* Binary scalars.
 */

int bb_decode_intle(int *dst,struct bb_decoder *decoder,int size) {
  int err=bb_intle_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_intbe(int *dst,struct bb_decoder *decoder,int size) {
  int err=bb_intbe_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_vlq(int *dst,struct bb_decoder *decoder) {
  int err=bb_vlq_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_vlq5(int *dst,struct bb_decoder *decoder) {
  int err=bb_vlq5_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_utf8(int *dst,struct bb_decoder *decoder) {
  int err=bb_utf8_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_fixed(double *dst,struct bb_decoder *decoder,int size,int fractbitc) {
  int err=bb_fixed_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size,fractbitc);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

int bb_decode_fixedf(float *dst,struct bb_decoder *decoder,int size,int fractbitc) {
  int err=bb_fixedf_decode(dst,(char*)decoder->src+decoder->srcp,decoder->srcc-decoder->srcp,size,fractbitc);
  if (err<1) return -1;
  decoder->srcp+=err;
  return err;
}

/* Decode raw data.
 */

int bb_decode_raw(void *dstpp,struct bb_decoder *decoder,int len) {
  if (len<0) return -1;
  if (len>bb_decoder_remaining(decoder)) return -1;
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int bb_decode_intlelen(void *dstpp,struct bb_decoder *decoder,int size) {
  int p0=decoder->srcp,len;
  if (bb_decode_intle(&len,decoder,size)<0) return -1;
  if (len>bb_decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int bb_decode_intbelen(void *dstpp,struct bb_decoder *decoder,int size) {
  int p0=decoder->srcp,len;
  if (bb_decode_intbe(&len,decoder,size)<0) return -1;
  if (len>bb_decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int bb_decode_vlqlen(void *dstpp,struct bb_decoder *decoder) {
  int p0=decoder->srcp,len;
  if (bb_decode_vlq(&len,decoder)<0) return -1;
  if (len>bb_decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

int bb_decode_vlq5len(void *dstpp,struct bb_decoder *decoder) {
  int p0=decoder->srcp,len;
  if (bb_decode_vlq5(&len,decoder)<0) return -1;
  if (len>bb_decoder_remaining(decoder)) { decoder->srcp=p0; return -1; }
  if (dstpp) *(void**)dstpp=(char*)decoder->src+decoder->srcp;
  decoder->srcp+=len;
  return len;
}

/* Signature assertion.
 */
 
int bb_decode_assert(struct bb_decoder *decoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (srcc>bb_decoder_remaining(decoder)) return -1;
  if (memcmp((char*)decoder->src+decoder->srcp,src,srcc)) return -1;
  decoder->srcp+=srcc;
  return 0;
}

/* Line of text.
 */
 
int bb_decode_line(void *dstpp,struct bb_decoder *decoder) {
  const char *src=(char*)decoder->src+decoder->srcp;
  int srcc=bb_decoder_remaining(decoder);
  int c=0;
  while (c<srcc) {
    if (src[c++]==0x0a) break;
  }
  decoder->srcp+=c;
  *(const void**)dstpp=src;
  return c;
}

/* Begin decoding a bit of JSON.
 * Asserts no sticky error, skips whitespace, asserts some content available.
 */
 
static int bb_decode_json_prepare(struct bb_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->srcp<decoder->srcc)&&(((unsigned char*)decoder->src)[decoder->srcp]<=0x20)) decoder->srcp++;
  if (decoder->srcp>=decoder->srcc) return decoder->jsonctx=-1;
  return 0;
}

/* Start JSON structure.
 */

int bb_decode_json_object_start(struct bb_decoder *decoder) {
  if (bb_decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='{') return decoder->jsonctx=-1;
  int jsonctx=decoder->jsonctx;
  decoder->srcp++;
  decoder->jsonctx='{';
  return jsonctx;
}

int bb_decode_json_array_start(struct bb_decoder *decoder) {
  if (bb_decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='[') return decoder->jsonctx=-1;
  int jsonctx=decoder->jsonctx;
  decoder->srcp++;
  decoder->jsonctx='[';
  return jsonctx;
}

/* End JSON structure.
 */
  
int bb_decode_json_object_end(struct bb_decoder *decoder,int jsonctx) {
  if (decoder->jsonctx!='{') return -1;
  if (bb_decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!='}') return decoder->jsonctx=-1;
  decoder->srcp++;
  decoder->jsonctx=jsonctx;
  return 0;
}

int bb_decode_json_array_end(struct bb_decoder *decoder,int jsonctx) {
  if (decoder->jsonctx!='[') return -1;
  if (bb_decode_json_prepare(decoder)<0) return -1;
  if (((char*)decoder->src)[decoder->srcp]!=']') return decoder->jsonctx=-1;
  decoder->srcp++;
  decoder->jsonctx=jsonctx;
  return 0;
}

/* Next field in JSON structure.
 */
 
int bb_decode_json_next(void *kpp,struct bb_decoder *decoder) {

  // This block means we accept and ignore redundant commas, in violation of the spec. I feel that's ok.
  while (1) {
    if (bb_decode_json_prepare(decoder)<0) return -1;
    if (((char*)decoder->src)[decoder->srcp]==',') decoder->srcp++;
    else break;
  }
  
  if (decoder->jsonctx=='{') {
    if (!kpp) return decoder->jsonctx=-1;
    if (((char*)decoder->src)[decoder->srcp]=='}') {
      decoder->srcp++;
      return 0;
    }
    const char *k=(char*)decoder->src+decoder->srcp;
    int kc=bb_string_measure(k,bb_decoder_remaining(decoder),0);
    if (kc<2) return decoder->jsonctx=-1;
    decoder->srcp+=kc;
    if (kc==2) { k+=1; kc-=2; } // drop quotes if not empty
    *(const char**)kpp=k;
    if (bb_decode_json_prepare(decoder)<0) return -1;
    if (((char*)decoder->src)[decoder->srcp++]!=':') return decoder->jsonctx=-1;
    return kc;
    
  } else if (decoder->jsonctx=='[') {
    if (kpp) return decoder->jsonctx=-1;
    return 1;
    
  } else return decoder->jsonctx=-1;
  return 0;
}

/* JSON primitives.
 */

int bb_decode_json_raw(void *dstpp,struct bb_decoder *decoder) {
  if (bb_decode_json_prepare(decoder)<0) return -1;
  const char *src=(char*)decoder->src+decoder->srcp;
  int c=bb_json_measure(src,bb_decoder_remaining(decoder));
  if (c<1) return decoder->jsonctx=-1;
  decoder->srcp+=c;
  
  while (c&&((unsigned char)src[c-1]<=0x20)) c--;
  while (c&&((unsigned char)src[0]<=0x20)) { c--; src++; }
  if (dstpp) *(const char**)dstpp=src;
  return c;
}

int bb_decode_json_skip(struct bb_decoder *decoder) {
  return bb_decode_json_raw(0,decoder);
}

int bb_decode_json_int(int *dst,struct bb_decoder *decoder) {
  const char *src=0;
  int srcc=bb_decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  if (bb_int_from_json(dst,src,srcc)<0) return decoder->jsonctx=-1;
  return 0;
}
  
int bb_decode_json_float(double *dst,struct bb_decoder *decoder) {
  const char *src=0;
  int srcc=bb_decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  if (bb_float_from_json(dst,src,srcc)<0) return decoder->jsonctx=-1;
  return 0;
}

int bb_decode_json_string(char *dst,int dsta,struct bb_decoder *decoder) {
  int p0=decoder->srcp;
  const char *src=0;
  int srcc=bb_decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  int dstc=bb_string_from_json(dst,dsta,src,srcc);
  if (dstc<0) return decoder->jsonctx=-1;
  if (dstc>dsta) decoder->srcp=p0;
  return dstc;
}

int bb_decode_json_string_to_encoder(struct bb_encoder *dst,struct bb_decoder *decoder) {
  const char *src=0;
  int srcc=bb_decode_json_raw(&src,decoder);
  if (srcc<0) return -1;
  while (1) {
    int err=bb_string_from_json(dst->v+dst->c,dst->a-dst->c,src,srcc);
    if (err<0) return decoder->jsonctx=-1;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      return 0;
    }
    if (bb_encoder_require(dst,err)<0) return -1;
  }
}

/* JSON peek.
 */

char bb_decode_json_get_type(const struct bb_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  const char *src=decoder->src;
  src+=decoder->srcp;
  int srcc=bb_decoder_remaining(decoder);
  while (srcc&&(((unsigned char)*src<=0x20)||(*src==','))) { src++; srcc--; }
  if (!srcc) return 0;
  switch (*src) {
    case '{': case '[': case '"': case 't': case 'f': case 'n': return *src;
    case '-': return '#';
  }
  if ((*src>='0')&&(*src<='9')) return '#';
  return 0;
}

/* Assert JSON completion.
 */
 
int bb_decode_json_done(const struct bb_decoder *decoder) {
  if (decoder->jsonctx) return -1;
  return 0;
}

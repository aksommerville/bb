/* bb_codec.h
 * General purpose decoder and encoder.
 */
 
#ifndef BB_CODEC_H
#define BB_CODEC_H

struct bb_decoder;
struct bb_encoder;

/* Decoder.
 * Cleanup is never necessary.
 * You can initialize inline, but must guarantee:
 *  - 0<=srcp<=srcc
 *  - (src) not null if (srcc>0)
 * Perfectly legal to copy a decoder directly, eg as a save point.
 ***************************************************************/
 
struct bb_decoder {
  const void *src;
  int srcc;
  int srcp;
  int jsonctx; // 0,-1,'{','['
};

static inline int bb_decoder_remaining(const struct bb_decoder *decoder) {
  return decoder->srcc-decoder->srcp;
}

/* Binary scalars.
 * (size) for int or fixed is in bytes, negative to read signed.
 * "vlq5" is VLQ with 5-byte sequences permitted, possibly dropping some high bits.
 * Regular "vlq" is MIDI compatible.
 */
int bb_decode_intle(int *dst,struct bb_decoder *decoder,int size);
int bb_decode_intbe(int *dst,struct bb_decoder *decoder,int size);
int bb_decode_vlq(int *dst,struct bb_decoder *decoder);
int bb_decode_vlq5(int *dst,struct bb_decoder *decoder);
int bb_decode_utf8(int *dst,struct bb_decoder *decoder);
int bb_decode_fixed(double *dst,struct bb_decoder *decoder,int size,int fractbitc);
int bb_decode_fixedf(float *dst,struct bb_decoder *decoder,int size,int fractbitc);

/* "Decode" a chunk of raw data, optionally with a length prefix.
 * These allow null (dstpp), if you're just skipping chunks.
 */
int bb_decode_raw(void *dstpp,struct bb_decoder *decoder,int len);
int bb_decode_intlelen(void *dstpp,struct bb_decoder *decoder,int size);
int bb_decode_intbelen(void *dstpp,struct bb_decoder *decoder,int size);
int bb_decode_vlqlen(void *dstpp,struct bb_decoder *decoder);
int bb_decode_vlq5len(void *dstpp,struct bb_decoder *decoder);

// Skip a chunk if it matches (src) exactly, or fail. eg for signatures
int bb_decode_assert(struct bb_decoder *decoder,const void *src,int srcc);

/* Return through the next LF. (implicitly handles CRLF nice too).
 */
int bb_decode_line(void *dstpp,struct bb_decoder *decoder);

/* JSON structures.
 * "start" returns (jsonctx) on success, which you must give back at "end".
 * "next" returns 0 if complete, >0 if a value is ready to decode, or <0 for error.
 * When "next" reports a value present, you must decode or skip it.
 * You don't have to handle errors at next; they are sticky.
 * You MUST provide a key vector for objects and MUST NOT for arrays.
 * We don't evaluate keys, rather we assume they will always be simple strings and just lop off the quotes.
 * If an empty key is found, we report it with the quotes (because empty would look like end-of-object).
 */
int bb_decode_json_object_start(struct bb_decoder *decoder);
int bb_decode_json_array_start(struct bb_decoder *decoder);
int bb_decode_json_object_end(struct bb_decoder *decoder,int jsonctx);
int bb_decode_json_array_end(struct bb_decoder *decoder,int jsonctx);
int bb_decode_json_next(void *kpp,struct bb_decoder *decoder);

/* JSON primitives.
 * "raw" returns some portion of the input text, corresponding to one JSON expression.
 * "skip" is exactly the same as "raw" but no return value.
 * decode_json_string() is special: It DOES NOT consume the token if it reports a length >dsta.
 * Anything can evaluate as anything -- see sr_*_from_json() in serial.h.
 */
int bb_decode_json_raw(void *dstpp,struct bb_decoder *decoder);
int bb_decode_json_skip(struct bb_decoder *decoder);
int bb_decode_json_int(int *dst,struct bb_decoder *decoder);
int bb_decode_json_float(double *dst,struct bb_decoder *decoder);
int bb_decode_json_string(char *dst,int dsta,struct bb_decoder *decoder);
int bb_decode_json_string_to_encoder(struct bb_encoder *dst,struct bb_decoder *decoder);

/* Peek at the next token and describe it:
 *   0: Invalid or sticky error.
 *   '{': Object
 *   '[': Array
 *   '"': String
 *   '#': Number (integer or float, we don't check)
 *   't': true
 *   'f': false
 *   'n': null
 * We only examine the first character of the token; it's not guaranteed to decode.
 */
char bb_decode_json_get_type(const struct bb_decoder *decoder);

// Assert successful completion. (note that this also succeeds if you never started decoding)
int bb_decode_json_done(const struct bb_decoder *decoder);

/* Encoder.
 * Cleanup is necessary, but you can also safely yoink (v) and not clean up.
 ***************************************************************/
 
struct bb_encoder {
  char *v;
  int c,a;
  int jsonctx; // 0,-1,'{','['
};

void bb_encoder_cleanup(struct bb_encoder *encoder);
int bb_encoder_require(struct bb_encoder *encoder,int addc);

int bb_encoder_replace(struct bb_encoder *encoder,int p,int c,const void *src,int srcc);

/* Append a chunk of raw data, optionally with a preceding length.
 * (srcc<0) to measure to the first NUL exclusive.
 */
int bb_encode_null(struct bb_encoder *encoder,int c);
int bb_encode_raw(struct bb_encoder *encoder,const void *src,int srcc);
int bb_encode_intlelen(struct bb_encoder *encoder,const void *src,int srcc,int size);
int bb_encode_intbelen(struct bb_encoder *encoder,const void *src,int srcc,int size);
int bb_encode_vlqlen(struct bb_encoder *encoder,const void *src,int srcc);
int bb_encode_vlq5len(struct bb_encoder *encoder,const void *src,int srcc);

/* Insert a word of the given type at the given position, value is from there to the end.
 * This is an alternative to encode_*len(), where the length is not known in advance.
 */
int bb_encoder_insert_intlelen(struct bb_encoder *encoder,int p,int size);
int bb_encoder_insert_intbelen(struct bb_encoder *encoder,int p,int size);
int bb_encoder_insert_vlqlen(struct bb_encoder *encoder,int p);
int bb_encoder_insert_vlq5len(struct bb_encoder *encoder,int p);

int bb_encode_fmt(struct bb_encoder *encoder,const char *fmt,...);

int bb_encode_intle(struct bb_encoder *encoder,int src,int size);
int bb_encode_intbe(struct bb_encoder *encoder,int src,int size);
int bb_encode_vlq(struct bb_encoder *encoder,int src);
int bb_encode_vlq5(struct bb_encoder *encoder,int src);
int bb_encode_utf8(struct bb_encoder *encoder,int src);
int bb_encode_fixed(struct bb_encoder *encoder,double src,int size,int fractbitc);
int bb_encode_fixedf(struct bb_encoder *encoder,float src,int size,int fractbitc);

int bb_encode_base64(struct bb_encoder *encoder,const void *src,int srcc);

// This does not interact with the JSON context; mostly it's used internally.
int bb_encode_json_string_token(struct bb_encoder *encoder,const char *src,int srcc);

/* JSON structures.
 * Same as decode, you must hold the (jsonctx) returned at "start", until the corresponding "end".
 * We pack JSON tightly. You are free to append spaces and newlines whenever you have control of the encoder.
 */
int bb_encode_json_object_start(struct bb_encoder *encoder,const char *k,int kc);
int bb_encode_json_array_start(struct bb_encoder *encoder,const char *k,int kc);
int bb_encode_json_object_end(struct bb_encoder *encoder,int jsonctx);
int bb_encode_json_array_end(struct bb_encoder *encoder,int jsonctx);

/* JSON primitives.
 * If immediately inside an object, you MUST provide a key.
 * Inside an array, or at global scope, you MUST NOT.
 * "preencoded" takes JSON text as the value and emits it verbatim -- keeping the format valid is up to you.
 */
int bb_encode_json_preencoded(struct bb_encoder *encoder,const char *k,int kc,const char *v,int vc);
int bb_encode_json_null(struct bb_encoder *encoder,const char *k,int kc);
int bb_encode_json_boolean(struct bb_encoder *encoder,const char *k,int kc,int v);
int bb_encode_json_int(struct bb_encoder *encoder,const char *k,int kc,int v);
int bb_encode_json_float(struct bb_encoder *encoder,const char *k,int kc,double v);
int bb_encode_json_string(struct bb_encoder *encoder,const char *k,int kc,const char *v,int vc);

// Assert completion (does not output anything).
int bb_encode_json_done(const struct bb_encoder *encoder);

#endif

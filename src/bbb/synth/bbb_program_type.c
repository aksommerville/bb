#include "bbb_synth_internal.h"

/* Global type registry.
 */

#define _(tag) extern const struct bbb_program_type bbb_program_type_##tag;
BBB_FOR_EACH_PROGRAM_TYPE
#undef _ 
 
static const struct bbb_program_type *bbb_program_typev[256]={
  [BBB_PROGRAM_TYPE_dummy]=&bbb_program_type_silent,
#define _(tag) [BBB_PROGRAM_TYPE_##tag]=&bbb_program_type_##tag,
BBB_FOR_EACH_PROGRAM_TYPE
#undef _
};

/* Get type from registry.
 */

const struct bbb_program_type *bbb_program_type_by_id(uint8_t ptid) {
  return bbb_program_typev[ptid];
}

#include "bbb/bbb.h"
#include <stdio.h>

/* Measure encoded program.
 */
 
int bbb_measure_program(const void *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  const uint8_t *SRC=src;
  uint8_t type=SRC[0];
  int srcp=1,err;
  switch (type) {
   
    #define FIXED(c) { \
      if (srcp>srcc-(c)) return -1; \
      srcp+=(c); \
    }
    #define SUBPROGRAM { \
      if ((err=bbb_measure_program(SRC+srcp,srcc-srcp))<0) return -1; \
      srcp+=err; \
    }
    #define ENV { \
      if ((err=bbb_measure_env(SRC+srcp,srcc-srcp))<0) return -1; \
      srcp+=err; \
    }

    // A few oddballs have no content...
    case BBB_PROGRAM_TYPE_dummy: return 1;
    case BBB_PROGRAM_TYPE_silent: return 1;
    
    case BBB_PROGRAM_TYPE_split: {
        while (srcp<srcc) {
          FIXED(3)
          SUBPROGRAM
        }
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_shape1: {
        FIXED(2)
        ENV
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_harm1: {
        FIXED(2)
        int coefc=(SRC[srcp-1]&0x0f)+1;
        FIXED(coefc)
        ENV
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_fm1: {
        FIXED(3)
        ENV
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_shapev: {
        FIXED(2)
        ENV
        ENV
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_harmv: {
        FIXED(2)
        int coefc=(SRC[srcp-1]&0x0f)+1;
        FIXED(coefc*2)
        ENV
        ENV
        return srcp;
      }
        
    case BBB_PROGRAM_TYPE_fmv: {
        FIXED(3)
        ENV
        ENV
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_weedrums: {
        FIXED(2)
        while (srcp<srcc) {
          if (!SRC[srcp++]) break;
          if (srcp>srcc-3) return -1;
          srcp+=3;
        }
        return srcp;
      }
      
    case BBB_PROGRAM_TYPE_cheapfx: {
        FIXED(2)
        while (1) {
          if (srcp>srcc-2) return -1;
          if (!SRC[srcp]&&!SRC[srcp+1]) { srcp+=2; break; }
          FIXED(6)
          ENV
          ENV
          ENV
        }
        return srcp;
      }
    
    #undef FIXED
    #undef SUBPROGRAM
    #undef ENV
    
    // Anything we don't specifically know, it's an error...
  }
  return -1;
}

/* Measure encoded envelope.
 */
 
int bbb_measure_env(const void *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  const uint8_t *SRC=src;
  
  // Short format: Fixed 3-byte length.
  if (SRC[0]&0x80) {
    if (srcc<3) return -1;
    return 3;
  }
  
  // Long format.
  int srcp=0;
  uint8_t flags=SRC[srcp++];
  int velocity=(flags&0x40);
  int sustain=(flags&0x20);
  int initlevel=(flags&0x10);
  int levelsize=(flags&0x08)?2:1;
  int timesize=(flags&0x04)?2:1;
  
  // Skip remainder of header, check length at the end of it.
  srcp+=2; // Level scale, time scale.
  if (sustain) srcp+=1;
  if (initlevel) {
    if (velocity) srcp+=levelsize*2;
    else srcp+=levelsize;
  }
  if (srcp>srcc) return -1;
  
  // Points.
  while (srcp<=srcc-levelsize) {
  
    // Terminate on a natural zero for timelo.
    if (timesize==2) {
      if (!SRC[srcp]&&!SRC[srcp+1]) {
        srcp+=2;
        break;
      }
    } else {
      if (!SRC[srcp]) {
        srcp+=1;
        break;
      }
    }
    
    srcp+=timesize;
    srcp+=levelsize;
    if (velocity) {
      srcp+=timesize;
      srcp+=levelsize;
    }
  }
  
  return srcp;
}

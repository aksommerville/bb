#include "bb_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Global type registry.
 */
 
extern const struct bb_driver_type bb_driver_type_alsa;
extern const struct bb_driver_type bb_driver_type_pulse;
 
static const struct bb_driver_type *bb_driver_typev[]={
#if BB_USE_alsa
  &bb_driver_type_alsa,
#endif
#if BB_USE_pulse
  &bb_driver_type_pulse,
#endif
//TODO alsa, mac, windows
  &bb_driver_type_silent,
};

/* Delete.
 */

void bb_driver_del(struct bb_driver *driver) {
  if (!driver) return;
  if (driver->refc-->1) return;
  if (driver->type->del) driver->type->del(driver);
  if (driver->type->singleton==driver) memset(driver,0,driver->type->objlen);
  else free(driver);
}

/* Retain.
 */
 
int bb_driver_ref(struct bb_driver *driver) {
  if (!driver) return -1;
  if (driver->refc<1) return -1;
  if (driver->refc==INT_MAX) return -1;
  driver->refc++;
  return 0;
}

/* New.
 */

struct bb_driver *bb_driver_new(
  const struct bb_driver_type *type,
  int rate,int chanc,
  int samplefmt,
  void (*cb)(void *v,int c,struct bb_driver *driver),
  void *userdata
) {
  if ((rate<1)||(rate>1000000)) return 0;
  if ((chanc<1)||(chanc>16)) return 0;
  if (!cb) return 0;
  
  if (!type) {
    if (!(type=bb_driver_type_by_index(0))) return 0;
  }
  
  struct bb_driver *driver=type->singleton;
  if (driver) {
    if (driver->refc) return 0;
  } else {
    if (!(driver=calloc(1,type->objlen))) return 0;
  }
  
  driver->type=type;
  driver->refc=1;
  driver->rate=rate;
  driver->chanc=chanc;
  driver->samplefmt=samplefmt;
  driver->cb=cb;
  driver->userdata=userdata;
  
  if (type->init) {
    if (type->init(driver)<0) {
      bb_driver_del(driver);
      return 0;
    }
  }
  
  return driver;
}

/* Hooks.
 */

int bb_driver_update(struct bb_driver *driver) {
  if (driver->type->update) return driver->type->update(driver);
  return 0;
}

int bb_driver_lock(struct bb_driver *driver) {
  if (driver->type->lock) return driver->type->lock(driver);
  return 0;
}

int bb_driver_unlock(struct bb_driver *driver) {
  if (driver->type->unlock) return driver->type->unlock(driver);
  return 0;
}

/* Access to type registry.
 */
 
const struct bb_driver_type *bb_driver_type_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(bb_driver_typev)/sizeof(void*);
  if (p>=c) return 0;
  return bb_driver_typev[p];
}

const struct bb_driver_type *bb_driver_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  const struct bb_driver_type **p=bb_driver_typev;
  int i=sizeof(bb_driver_typev)/sizeof(void*);
  for (;i-->0;p++) {
    const struct bb_driver_type *type=*p;
    if (memcmp(type->name,name,namec)) continue;
    if (type->name[namec]) continue;
    return type;
  }
  return 0;
}

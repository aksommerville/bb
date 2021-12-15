#include "bb_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* MIDI driver type registry.
 */
 
extern const struct bb_midi_driver_type bb_midi_driver_type_ossmidi;
 
static const struct bb_midi_driver_type *bb_midi_driver_typev[]={
#if BB_USE_ossmidi
  &bb_midi_driver_type_ossmidi,
#endif
  &bb_midi_driver_type_noop,
};

/* Get type from registry.
 */
 
const struct bb_midi_driver_type *bb_midi_driver_type_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(bb_midi_driver_typev)/sizeof(void*);
  if (p>=c) return 0;
  return bb_midi_driver_typev[p];
}

const struct bb_midi_driver_type *bb_midi_driver_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  int i=sizeof(bb_midi_driver_typev)/sizeof(void*);
  const struct bb_midi_driver_type **p=bb_midi_driver_typev;
  for (;i-->0;p++) {
    if (memcmp((*p)->name,name,namec)) continue;
    if ((*p)->name[namec]) continue;
    return *p;
  }
  return 0;
}

/* Dummy driver.
 */

const struct bb_midi_driver_type bb_midi_driver_type_noop={
  .name="noop",
  .objlen=sizeof(struct bb_midi_driver),
};

/* Wrapper.
 */

void bb_midi_driver_del(struct bb_midi_driver *driver) {
  if (!driver) return;
  if (driver->refc-->1) return;
  if (driver->type->del) driver->type->del(driver);
  if (driver->type->singleton==driver) memset(driver,0,driver->type->objlen);
  else free(driver);
}

int bb_midi_driver_ref(struct bb_midi_driver *driver) {
  if (!driver) return -1;
  if (driver->refc<1) return -1;
  if (driver->refc==INT_MAX) return -1;
  driver->refc++;
  return 0;
}

struct bb_midi_driver *bb_midi_driver_new(
  const struct bb_midi_driver_type *type,
  void (*cb)(const void *src,int srcc,int devid,struct bb_midi_driver *driver),
  void *userdata
) {
  if (!type) {
    if (!(type=bb_midi_driver_type_by_index(0))) return 0;
  }
  
  struct bb_midi_driver *driver=type->singleton;
  if (driver) {
    if (driver->refc) return 0;
  } else {
    if (!(driver=calloc(1,type->objlen))) return 0;
  }
  
  driver->refc=1;
  driver->type=type;
  driver->cb=cb;
  driver->userdata=userdata;
  
  if (type->init&&(type->init(driver)<0)) {
    bb_midi_driver_del(driver);
    return 0;
  }
  
  return driver;
}

int bb_midi_driver_update(struct bb_midi_driver *driver) {
  if (!driver||!driver->type->update) return 0;
  return driver->type->update(driver);
}

int bb_midi_driver_get_device_name(char *dst,int dsta,struct bb_midi_driver *driver,int devid) {
  if (!driver||!driver->type->get_device_name) return -1;
  return driver->type->get_device_name(dst,dsta,driver,devid);
}

int bb_midi_driver_count_devices(const struct bb_midi_driver *driver) {
  if (!driver||!driver->type->count_devices) return 0;
  return driver->type->count_devices(driver);
}

int bb_midi_driver_devid_by_index(const struct bb_midi_driver *driver,int p) {
  if (p<0) return -1;
  if (!driver||!driver->type->devid_by_index) return -1;
  return driver->type->devid_by_index(driver,p);
}

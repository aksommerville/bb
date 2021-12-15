#include "bb_ossmidi_internal.h"

/* Cleanup.
 */
 
void bb_ossmidi_device_cleanup(struct bb_ossmidi_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->name) free(device->name);
}
 
static void _bb_ossmidi_del(struct bb_midi_driver *driver) {
  if (DRIVER->infd>=0) close(DRIVER->infd);
  if (DRIVER->pollfdv) free(DRIVER->pollfdv);
  if (DRIVER->devicev) {
    while (DRIVER->devicec-->0) {
      bb_ossmidi_device_cleanup(DRIVER->devicev+DRIVER->devicec);
    }
    free(DRIVER->devicev);
  }
}

/* Init.
 */
 
static int _bb_ossmidi_init(struct bb_midi_driver *driver) {

  if ((DRIVER->infd=inotify_init())<0) {
    fprintf(stderr,"ossmidi: Failed to open inotify. We will not detect newly-connected devices.\n");
  } else {
    inotify_add_watch(DRIVER->infd,BB_OSSMIDI_PATH,IN_CREATE|IN_ATTRIB);
  }
  DRIVER->refresh=1;

  return 0;
}

/* Update.
 */
 
static int _bb_ossmidi_update(struct bb_midi_driver *driver) {

  if (DRIVER->refresh) {
    DRIVER->refresh=0;
    if (bb_ossmidi_scan(driver)<0) return -1;
  }
  
  if (bb_ossmidi_rebuild_pollfdv(driver)<0) return -1;
  if (!DRIVER->pollfdc) return 0;
  if (poll(DRIVER->pollfdv,DRIVER->pollfdc,0)<=0) return 0;
  
  struct pollfd *pollfd=DRIVER->pollfdv;
  int i=DRIVER->pollfdc;
  for (;i-->0;pollfd++) {
    if (pollfd->revents) {
      if (bb_ossmidi_read_file(driver,pollfd->fd)<0) {
        bb_ossmidi_drop_file(driver,pollfd->fd);
      }
    }
  }

  return 0;
}

/* Get device name.
 */
 
static int _bb_ossmidi_get_device_name(char *dst,int dsta,struct bb_midi_driver *driver,int devid) {
  struct bb_ossmidi_device *device=bb_ossmidi_device_by_devid(driver,devid);
  if (!device) return -1;
  
  if (!device->namec) {
    if (bb_ossmidi_device_fetch_name(driver,device)<0) return -1;
  }
  
  if (device->namec<=dsta) {
    memcpy(dst,device->name,device->namec);
    if (device->namec<dsta) dst[device->namec]=0;
  }
  return device->namec;
}

/* Count devices.
 */
 
static int _bb_ossmidi_count_devices(const struct bb_midi_driver *driver) {
  return DRIVER->devicec;
}

/* Devid by index.
 */
 
static int _bb_ossmidi_devid_by_index(const struct bb_midi_driver *driver,int p) {
  if ((p<0)||(p>=DRIVER->devicec)) return -1;
  return DRIVER->devicev[p].devid;
}

/* Type definition.
 */
 
static struct bb_midi_driver_ossmidi _bb_ossmidi_singleton={0};

const struct bb_midi_driver_type bb_midi_driver_type_ossmidi={
  .name="ossmidi",
  .objlen=sizeof(struct bb_midi_driver_ossmidi),
  .singleton=&_bb_ossmidi_singleton,
  .del=_bb_ossmidi_del,
  .init=_bb_ossmidi_init,
  .update=_bb_ossmidi_update,
  .get_device_name=_bb_ossmidi_get_device_name,
  .count_devices=_bb_ossmidi_count_devices,
  .devid_by_index=_bb_ossmidi_devid_by_index,
};

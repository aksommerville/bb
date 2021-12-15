#ifndef BB_OSSMIDI_INTERNAL_H
#define BB_OSSMIDI_INTERNAL_H

#include "driver/bb_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/poll.h>

#define BB_OSSMIDI_PATH "/dev/"

struct bb_midi_driver_ossmidi {
  struct bb_midi_driver hdr;
  int infd;
  int refresh; // nonzero to scan directory at the next update
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
  struct bb_ossmidi_device {
    int fd;
    int devid;
    int ossid;
    char *name;
    int namec;
  } *devicev;
  int devicec,devicea;
};

#define DRIVER ((struct bb_midi_driver_ossmidi*)driver)

void bb_ossmidi_device_cleanup(struct bb_ossmidi_device *device);
int bb_ossmidi_scan(struct bb_midi_driver *driver);
int bb_ossmidi_rebuild_pollfdv(struct bb_midi_driver *driver);
int bb_ossmidi_read_file(struct bb_midi_driver *driver,int fd);
int bb_ossmidi_drop_file(struct bb_midi_driver *driver,int fd);
int bb_ossmidi_device_index_by_fd(const struct bb_midi_driver *driver,int fd);
struct bb_ossmidi_device *bb_ossmidi_device_by_fd(const struct bb_midi_driver *driver,int fd);
struct bb_ossmidi_device *bb_ossmidi_device_by_devid(const struct bb_midi_driver *driver,int devid);
struct bb_ossmidi_device *bb_ossmidi_device_by_ossid(const struct bb_midi_driver *driver,int ossid);
int bb_ossmidi_device_fetch_name(struct bb_midi_driver *driver,struct bb_ossmidi_device *device);

#endif

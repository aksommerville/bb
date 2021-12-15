#include "bb_ossmidi_internal.h"
#include "share/bb_codec.h"
#include "share/bb_fs.h"

/* Generate a valid unused devid.
 * (it's always possible).
 */
 
static int bb_ossmidi_unused_devid(const struct bb_midi_driver *driver) {
  if (DRIVER->devicec<1) return 1;
  const struct bb_ossmidi_device *device=DRIVER->devicev;
  int i=DRIVER->devicec,lo=device->devid,hi=device->devid;
  for (;i-->0;device++) {
    if (device->devid<lo) lo=device->devid;
    else if (device->devid>hi) hi=device->devid;
  }
  // Prefer to go higher, to an ID we haven't used yet.
  // It is *extremely* unlikely that any other case would be needed.
  if (hi<INT_MAX) return hi+1;
  // OK, you've connected 2 billion devices since startup, impressive.
  if (lo>1) return lo-1;
  // You've connected one device, then leap-frog-connected two others to drive the ID up to INT_MAX.
  // Just what are you playing at, user?
  // There must be a gap somewhere (because sizeof(struct bb_ossmidi_device)>2). Find it.
  int devid=2; for (;;devid++) {
    if (!bb_ossmidi_device_by_devid(driver,devid)) return devid;
  }
  return 1;
}

/* New file detected in device directory.
 */
 
static int bb_ossmidi_consider_file(struct bb_midi_driver *driver,const char *path,const char *base,char type) {
  
  // Does it look like an OSS MIDI device?
  if (!path) return 0;
  if (!base) {
    int i=0; for (;path[i];i++) {
      if (path[i]=='/') base=path+i+1;
    }
  }
  if (memcmp(base,"midi",4)) return 0;
  if (!type) type=bb_file_get_type(path);
  if (type!='c') return 0;
  int ossid=0,basep=4;
  for (;base[basep];basep++) {
    int digit=base[basep]-'0';
    if ((digit<0)||(digit>9)) return 0;
    ossid*=10;
    ossid+=digit;
    if (ossid>999) return 0;
  }
  
  // Have we already got it?
  if (bb_ossmidi_device_by_ossid(driver,ossid)) return 0;
  
  // Make room in the list.
  if (DRIVER->devicec>=DRIVER->devicea) {
    int na=DRIVER->devicea+4;
    if (na>INT_MAX/sizeof(struct bb_ossmidi_device)) return -1;
    void *nv=realloc(DRIVER->devicev,sizeof(struct bb_ossmidi_device)*na);
    if (!nv) return -1;
    DRIVER->devicev=nv;
    DRIVER->devicea=na;
  }
  
  // Open file. If we fail due to permissions or whatever, that's fine, forget it.
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  int devid=bb_ossmidi_unused_devid(driver);
  
  // Record it.
  struct bb_ossmidi_device *device=DRIVER->devicev+DRIVER->devicec++;
  memset(device,0,sizeof(struct bb_ossmidi_device));
  device->fd=fd;
  device->devid=devid;
  device->ossid=ossid;
  
  // Send the welcome packet.
  if (driver->cb) {
    driver->cb("\xf0\xf7",2,devid,driver);
  }
  
  return 1;
}

/* Scan directory for new devices.
 */
 
static int bb_ossmidi_scan_cb(const char *path,const char *base,char type,void *userdata) {
  struct bb_midi_driver *driver=userdata;
  return bb_ossmidi_consider_file(driver,path,base,type);
}
 
int bb_ossmidi_scan(struct bb_midi_driver *driver) {
  if (bb_dir_read(BB_OSSMIDI_PATH,bb_ossmidi_scan_cb,driver)<0) return -1;
  return 0;
}

/* Rebuild pollfd list.
 */
 
static int bb_ossmidi_add_pollfd(struct bb_midi_driver *driver,int fd) {
  if (DRIVER->pollfdc>=DRIVER->pollfda) {
    int na=DRIVER->pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(DRIVER->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    DRIVER->pollfdv=nv;
    DRIVER->pollfda=na;
  }
  struct pollfd *pollfd=DRIVER->pollfdv+DRIVER->pollfdc++;
  memset(pollfd,0,sizeof(struct pollfd));
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  return 0;
}
 
int bb_ossmidi_rebuild_pollfdv(struct bb_midi_driver *driver) {
  DRIVER->pollfdc=0;
  if (DRIVER->infd>=0) {
    if (bb_ossmidi_add_pollfd(driver,DRIVER->infd)<0) return -1;
  }
  struct bb_ossmidi_device *device=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;device++) {
    if (bb_ossmidi_add_pollfd(driver,device->fd)<0) return -1;
  }
  return 0;
}

/* Receive content from inotify.
 */
 
static int bb_ossmidi_receive_inotify(struct bb_midi_driver *driver,const char *src,int srcc) {
  char subpath[1024];
  int subpathc=snprintf(subpath,sizeof(subpath),BB_OSSMIDI_PATH);
  if ((subpathc<0)||(subpathc>=sizeof(subpath))) return -1;
  
  int srcp=0,stopp=srcc-sizeof(struct inotify_event);
  while (srcp<=stopp) {
    struct inotify_event *event=(struct inotify_event*)(src+srcp);
    srcp+=sizeof(struct inotify_event);
    if (srcp>srcc-event->len) return -1;
    const char *base=src+srcp;
    srcp+=event->len;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (subpathc>=sizeof(subpath)-basec) return -1;
    memcpy(subpath+subpathc,base,basec);
    subpath[subpathc+basec]=0;
    if (bb_ossmidi_consider_file(driver,subpath,base,0)<0) return -1;
  }
  return 0;
}

/* Receive content from a device.
 */
 
static int bb_ossmidi_receive_device(struct bb_midi_driver *driver,struct bb_ossmidi_device *device,const void *src,int srcc) {
  if (driver->cb) {
    // Don't send either of the two special packets. Crazy unlikely, but let's be certain.
    if (!srcc) ; // ...actually this is not even possible
    else if ((srcc==2)&&!memcmp(src,"\xf0\xf7",2)) ;
    else driver->cb(src,srcc,device->devid,driver);
  }
  return 0;
}

/* Read file.
 */
 
int bb_ossmidi_read_file(struct bb_midi_driver *driver,int fd) {
  char buf[1024];
  int bufc=read(fd,buf,sizeof(buf));
  if (bufc<=0) return -1;
  if (fd==DRIVER->infd) return bb_ossmidi_receive_inotify(driver,buf,bufc);
  struct bb_ossmidi_device *device=bb_ossmidi_device_by_fd(driver,fd);
  if (device) return bb_ossmidi_receive_device(driver,device,buf,bufc);
  return -1;
}

/* Drop file.
 */
 
int bb_ossmidi_drop_file(struct bb_midi_driver *driver,int fd) {
  if (fd<0) return 0;
  
  if (fd==DRIVER->infd) {
    fprintf(stderr,"ossmidi: Lost inotify connection. Newly-connected devices will not be detected.\n");
    close(fd);
    DRIVER->infd=-1;
    return 0;
  }
  
  int p=bb_ossmidi_device_index_by_fd(driver,fd);
  if (p>=0) {
    struct bb_ossmidi_device *device=DRIVER->devicev+p;
    int devid=device->devid;
    bb_ossmidi_device_cleanup(device);
    DRIVER->devicec--;
    memmove(device,device+1,sizeof(struct bb_ossmidi_device)*(DRIVER->devicec-p));
    if (driver->cb) driver->cb("",0,devid,driver);
    return 0;
  }
  
  fprintf(stderr,"ossmidi: Unknown file %d\n",fd);
  return 0;
}

/* Find a device in the list.
 */
 
int bb_ossmidi_device_index_by_fd(const struct bb_midi_driver *driver,int fd) {
  int i=DRIVER->devicec;
  struct bb_ossmidi_device *device=DRIVER->devicev+i-1;
  for (;i-->0;device--) {
    if (device->fd==fd) return i;
  }
  return -1;
}
 
struct bb_ossmidi_device *bb_ossmidi_device_by_fd(const struct bb_midi_driver *driver,int fd) {
  struct bb_ossmidi_device *device=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;device++) {
    if (device->fd==fd) return device;
  }
  return 0;
}

struct bb_ossmidi_device *bb_ossmidi_device_by_devid(const struct bb_midi_driver *driver,int devid) {
  struct bb_ossmidi_device *device=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;device++) {
    if (device->devid==devid) return device;
  }
  return 0;
}

struct bb_ossmidi_device *bb_ossmidi_device_by_ossid(const struct bb_midi_driver *driver,int ossid) {
  struct bb_ossmidi_device *device=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;device++) {
    if (device->ossid==ossid) return device;
  }
  return 0;
}

/* Device name.
 */
 
static int bb_ossmidi_find_device_name(char *dst,int dsta,int ossid,const char *path) {
  if (!path||!path[0]) {
    int dstc=0;
    if ((dstc=bb_ossmidi_find_device_name(dst,dsta,ossid,"/proc/asound/oss/sndstat"))>=0) return dstc;
    if ((dstc=bb_ossmidi_find_device_name(dst,dsta,ossid,"/dev/sndstat"))>=0) return dstc;
    return -1;
  }
  char *src=0;
  int srcc=bb_file_read(&src,path);
  if ((srcc<0)||!src) return -1;
  int readingdevices=0;
  struct bb_decoder decoder={.src=src,.srcc=srcc};
  const char *line;
  int linec;
  while ((linec=bb_decode_line(&line,&decoder))>0) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    
    if (readingdevices) {
      if (!linec) break; // end of "Midi devices:" block.
      int linedevid=0,linep=0;
      while ((linep<linec)&&(line[linep]>='0')&&(line[linep]<='9')) {
        linedevid*=10;
        linedevid+=line[linep]-'0';
        linep++;
      }
      if (!linep) break; // unexpected line in "Midi devices:" block, stop reading.
      if (linedevid==ossid) {
        if ((linep<linec)&&(line[linep]==':')) linep++;
        while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
        int dstc=linec-linep;
        if (dstc<=dsta) {
          memcpy(dst,line+linep,dstc);
          if (dstc<dsta) dst[dstc]=0;
        }
        return dstc;
      }
      
    } else if ((linec==13)&&!memcmp(line,"Midi devices:",13)) {
      readingdevices=1;
    }
  }
  free(src);
  return -1;
}
 
static int bb_ossmidi_device_set_name(struct bb_ossmidi_device *device,const char *src,int srcc) {
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=srcc;
  return 0;
}
 
int bb_ossmidi_device_fetch_name(struct bb_midi_driver *driver,struct bb_ossmidi_device *device) {
  char tmp[64];
  int tmpc;
  
  if ((tmpc=bb_ossmidi_find_device_name(tmp,sizeof(tmp),device->ossid,0))>0) {
    if (tmpc<=sizeof(tmp)) return bb_ossmidi_device_set_name(device,tmp,tmpc);
  }
  
  if ((tmpc=snprintf(tmp,sizeof(tmp),"%smidi%d",BB_OSSMIDI_PATH,device->ossid))>0) {
    if (tmpc<sizeof(tmp)) return bb_ossmidi_device_set_name(device,tmp,tmpc);
  }
  
  if ((tmpc=snprintf(tmp,sizeof(tmp),"device %d",device->devid))>0) {
    if (tmpc<sizeof(tmp)) return bb_ossmidi_device_set_name(device,tmp,tmpc);
  }
  
  return -1;
}

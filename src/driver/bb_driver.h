/* bb_driver.h
 * Generic interface to access the host's PCM output.
 * This is optional and completely separate from the synthesizer logic.
 */
 
#ifndef BB_DRIVER_H
#define BB_DRIVER_H

/* PCM-Out aka plain old "driver".
 ******************************************************************/

struct bb_driver_type;
struct bb_driver;

#define BB_SAMPLEFMT_SINT16     1
#define BB_SAMPLEFMT_FLOAT      2

struct bb_driver {
  const struct bb_driver_type *type;
  int refc;
  int rate;
  int chanc;
  int samplefmt;
  void (*cb)(void *v,int c,struct bb_driver *driver);
  void *userdata;
};

void bb_driver_del(struct bb_driver *driver);
int bb_driver_ref(struct bb_driver *driver);

/* Drivers are not required to accept the (rate,chanc,samplefmt) you request.
 * Check the driver after creation, to verify you can handle its selections.
 * (type) may be null for the default.
 */
struct bb_driver *bb_driver_new(
  const struct bb_driver_type *type,
  int rate,int chanc,
  int samplefmt,
  void (*cb)(void *v,int c,struct bb_driver *driver),
  void *userdata
);

int bb_driver_update(struct bb_driver *driver);
int bb_driver_lock(struct bb_driver *driver);
int bb_driver_unlock(struct bb_driver *driver);

struct bb_driver_type {
  const char *name;
  int objlen;
  void *singleton; // If set, we will not allocate, and only one can exist at a time.
  void (*del)(struct bb_driver *driver);
  int (*init)(struct bb_driver *driver);
  
  /* Typically, a multithreaded driver implements just (lock,unlock),
   * and a single-threaded one just (update).
   */
  int (*update)(struct bb_driver *driver);
  int (*lock)(struct bb_driver *driver);
  int (*unlock)(struct bb_driver *driver);
};

/* The driver at index zero should be the preferred default.
 * That's what you get if you try to instantiate without providing a type.
 */
const struct bb_driver_type *bb_driver_type_by_index(int p);
const struct bb_driver_type *bb_driver_type_by_name(const char *name,int namec);

// Tracks real time, triggers your callback synchronously at update, discards your output.
extern const struct bb_driver_type bb_driver_type_silent;

/* MIDI-In.
 ************************************************************/
 
struct bb_midi_driver_type;
struct bb_midi_driver;

struct bb_midi_driver {
  const struct bb_midi_driver_type *type;
  int refc;
  void (*cb)(const void *src,int srcc,int devid,struct bb_midi_driver *driver);
  void *userdata;
};

void bb_midi_driver_del(struct bb_midi_driver *driver);
int bb_midi_driver_ref(struct bb_midi_driver *driver);

/* One callback is used for all devices.
 * Each device is distinguished by a positive 'devid' unique across currently-connected devices.
 * On connection, driver should first send an empty Sysex packet: 0xf0 0xf7
 * On disconnection, driver should send an empty packet.
 * Those two payloads should not be sent in any other case.
 */
struct bb_midi_driver *bb_midi_driver_new(
  const struct bb_midi_driver_type *type,
  void (*cb)(const void *src,int srcc,int devid,struct bb_midi_driver *driver),
  void *userdata
);

/* MIDI drivers expose a synchronous single-threaded API.
 * If the underlying driver works multi-threaded, the driver is responsible 
 * for gathering input and reporting it during your synchronous update.
 */
int bb_midi_driver_update(struct bb_midi_driver *driver);

/* Access to device list and properties.
 */
int bb_midi_driver_get_device_name(char *dst,int dsta,struct bb_midi_driver *driver,int devid);
int bb_midi_driver_count_devices(const struct bb_midi_driver *driver);
int bb_midi_driver_devid_by_index(const struct bb_midi_driver *driver,int p);

struct bb_midi_driver_type {
  const char *name;
  int objlen;
  void *singleton;
  void (*del)(struct bb_midi_driver *driver);
  int (*init)(struct bb_midi_driver *driver);
  
  int (*update)(struct bb_midi_driver *driver);
  
  int (*get_device_name)(char *dst,int dsta,struct bb_midi_driver *driver,int devid);
  int (*count_devices)(const struct bb_midi_driver *driver);
  int (*devid_by_index)(const struct bb_midi_driver *driver,int p);
};

const struct bb_midi_driver_type *bb_midi_driver_type_by_index(int p);
const struct bb_midi_driver_type *bb_midi_driver_type_by_name(const char *name,int namec);

// Never sends events.
extern const struct bb_midi_driver_type bb_midi_driver_type_noop;

#endif

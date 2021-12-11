/* bb_driver.h
 * Generic interface to access the host's PCM output.
 * This is optional and completely separate from the synthesizer logic.
 * TODO MIDI in. Maybe also PCM-in and MIDI-out?
 */
 
#ifndef BB_DRIVER_H
#define BB_DRIVER_H

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

#endif

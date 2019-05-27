// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include "priq.h"
#include <stdio.h>
#include <sys/time.h>

// --- timeout struct ---

// enum for timeout types
enum tmo_typ{HEARTBEAT=0,CLIENT=1};

// timeout class
struct tmo_t{
  enum tmo_typ typ_;      // type of timeout (child process or heartbeat)
  size_t sec_;            // timeout in seconds
  size_t key_;            // key which can be used by client code to correlate the timeout with something
  size_t sec2tmo_;        // seconds from epoch until timer pops
};
struct tmo_t*tmo_ctor(enum tmo_typ typ,size_t sec,size_t key);      // constructor
void tmo_dtor(struct tmo_t*tmo);                                    // destructor
void tmo_dump(struct tmo_t*tmo,FILE*fp,int nl);                     // print timeout
enum tmo_typ tmo_type(struct tmo_t*tmo);                            // type of timeout
size_t tmo_sec(struct tmo_t*tmo);                                   // get timeout in sec
size_t tmo_key(struct tmo_t*tmo);                                   // get key stored in timeout
void tmo_deactivate(struct tmo_t*tmo);                              // deactivate a timeout
size_t tmo_sec2tmo(struct tmo_t*tmo);                               // seconds until timer pops (counted from epoch)
struct tmo_t*tmo_reactivate(struct tmo_t*tmo);                      // activate timer - i.e., set 'sec2tmo' value relative to 'now'
char const*const tmo_type2str(struct tmo_t*tmo);                    // get tmo type as a string

// --- timer queue ---
// (wrapper around 'priq')

struct priq*tmoq_ctor(size_t maxel);                                  // time queue constructor
void tmoq_dtor(struct priq*q);                                        // timer queue destructor
struct tmo_t*tmoq_front(struct priq*q);                               // get next timeout
void tmoq_pop(struct priq*q);                                         // pop queue
void tmoq_push(struct priq*q,struct tmo_t*tmo);                       // push a timeout on queue
struct timespec*tmoq_select_timeout(struct priq*q,struct timespec*tv);// get a pointer to next timeout
void tmoq_remove(struct priq*q,struct tmo_t*tmo);                     // remove tmo from queue

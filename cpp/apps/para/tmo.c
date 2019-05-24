// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "tmo.h"
#include "sys.h"
#include "error.h"
#include "util.h"
#include <time.h>

// --- timeout struct ---

// constructor
struct tmo_t*tmo_ctor(enum tmo_typ typ,size_t sec,size_t key){
  struct tmo_t*ret=emalloc(sizeof(struct tmo_t));
  ret->typ_=typ;                // type of timer - we support HEARTBEAT and CLIENT timers
  ret->stat_=ACTIVE;            // is timer active or should it be ignored
  ret->sec_=sec;                // timer value is in seconds
  ret->key_=key;                // a user defined key - typically an index into some table
  return tmo_reactivate(ret);   // activate timer
}
// destructor
void tmo_dtor(struct tmo_t*tmo){
  free(tmo);
}
// print timeout
void tmo_dump(struct tmo_t*tmo,FILE*fp,int nl){
  fprintf(fp,"type: %s, sec: %lu, state: %s, key: %lu, sec2tmo: %lu",
          (tmo->typ_==HEARTBEAT?"HEARTBEAT":"CLIENT"),
          tmo->sec_,
          (tmo->stat_==ACTIVE?"ACTIVE":"DEACTIVE"),
          tmo->key_,
          tmo->sec2tmo_);
  if(nl)fprintf(fp,"\n");
}
// type of timeout
enum tmo_typ tmo_type(struct tmo_t*tmo){
  return tmo->typ_;
}
// state of timeout
enum tmo_stat tmo_state(struct tmo_t*tmo){
  return tmo->stat_;
}
// get timeout in sec
size_t tmo_sec(struct tmo_t*tmo){
  return tmo->sec_;
}
// get key stored in timeout
size_t tmo_key(struct tmo_t*tmo){
  return tmo->key_;
}
// deactivate a timeout
void tmo_deactivate(struct tmo_t*tmo){
  tmo->stat_=DEACTIVE;
}
// seconds until timer pops
size_t tmo_sec2tmo(struct tmo_t*tmo){
  return tmo->sec2tmo_;
}
// activate timer - i.e., set 'sec2tmo' value relative to 'now'
struct tmo_t*tmo_reactivate(struct tmo_t*tmo){
  tmo->sec2tmo_=time(NULL)+tmo->sec_;
  return tmo;
}
// get tmo type as a string
char const*const tmo_type2str(struct tmo_t*tmo){
  return tmo_type(tmo)==HEARTBEAT?"HEARTBEAT":"CLIENT";
}

// --- timer queue ---

// timeout comparator
static int tmocmp(void*t1,void*t2){
  struct tmo_t*tt1=t1;
  struct tmo_t*tt2=t2;
  return tmo_sec(tt1)<=tmo_sec(tt2);
}
// (implemented as a priority queue)
struct priq*tmoq_ctor(size_t maxel){
  return priq_ctor(maxel,tmocmp);
}
// timer queue destructor
void tmoq_dtor(struct priq*q){
  while(priq_size(q)){
    struct tmo_t*t=priq_top(q);
    priq_pop(q);
    tmo_dtor(t);
  }
  priq_dtor(q);
}
// purge top of queue from DEACTIVE timeouts
// (we skip all timeouts that have been deactivated and return NULL if no timers found)
// (deactive timeouts are destroyed when found)
void qtmo_purge(struct priq*q){
  while(priq_size(q)){
    struct tmo_t*tmo=priq_top(q);
    if(tmo_state(tmo)==ACTIVE)return;
    priq_pop(q);
    tmo_dtor(tmo);
  }
}
// get next timeout
struct tmo_t*tmoq_front(struct priq*q){
  qtmo_purge(q);
  if(priq_size(q)==0)return NULL;
  return priq_top(q);
}
// pop queue
// (popped element is not being destroyed)
void tmoq_pop(struct priq*q){
  qtmo_purge(q);
  if(priq_size(q)>0)priq_pop(q);
}
// push a timeout on queue
void tmoq_push(struct priq*q,struct tmo_t*tmo){
  if(priq_full(q))app_message(FATAL,"attempt to push timeout on full tmoq in tmoq_push()");
  priq_push(q,tmo);
}
// get next timeout in format that can be passed to select() call
// (note: the second parameter must point to a non-null struct that will be filled in)
struct timespec*tmoq_select_timeout(struct priq*q,struct timespec*ts){
  if(ts==0)app_message(FATAL,"'ts' must not be null in tmoq_next_timeout()");
  if(!priq_size(q))return NULL;                       // if no timers we return null pointer
  struct tmo_t*tmo=tmoq_front(q);                     // get next timer
  if(tmo_sec2tmo(tmo)>time(NULL)){
    ts->tv_sec=maxulong(0,tmo_sec2tmo(tmo)-time(NULL)); // select() timout are in seconds from now
  }else{
    ts->tv_sec=0;
  }
  ts->tv_nsec=0;                                      // granularity is in seconds
  return ts;
}
// remove q timer from queue
void tmoq_remove(struct priq*q,struct tmo_t*tmo){
  priq_remove(q,tmo);
}

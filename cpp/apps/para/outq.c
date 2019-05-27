// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "outq.h"
#include "priq.h"
#include "combuf.h"
#include "sys.h"
#include "error.h"
#include "util.h"

// combuf comparator
// (compares line numbers)
static int combufcmp(void*cb1,void*cb2){
  struct combuf*ccb1=cb1;
  struct combuf*ccb2=cb2;
  return combuf_lineno(ccb1)<combuf_lineno(ccb2);
}
// time queue constructor
struct outq_t*outq_ctor(size_t maxel,size_t inc,int startlineno){
  struct outq_t*ret=emalloc(sizeof(struct outq_t));
  ret->nextlineno_=startlineno;
  ret->pq_=priq_ctor(maxel,inc,combufcmp);
  return ret;
}
// output queue destructor
void outq_dtor(struct outq_t*q){
  struct priq*pq=q->pq_;
  while(priq_size(pq)){
    struct combuf*cb=priq_top(pq);
    priq_pop(pq);
    combuf_dtor(cb);
  }
  priq_dtor(pq);
  free(q);
}
// get next combuf for output
struct combuf*outq_front(struct outq_t*q){
  if(priq_size(q->pq_)==0)return NULL;
  return priq_top(q->pq_);
}
// push a combuf on queue
void outq_push(struct outq_t*q,struct combuf*cb){
  priq_push(q->pq_,cb);
}
// pop queue
void outq_pop(struct outq_t*q){
  if(priq_size(q->pq_)==0)app_message(FATAL,"attempt to pop empty output queue in outq_pop()");
  priq_pop(q->pq_);
  ++q->nextlineno_;
}
// true if top element on queue is ready (has correct line number) to be written
int outq_ready(struct outq_t*q){
  struct combuf*cb=outq_front(q);
  if(!cb)return 0;
  return combuf_lineno(cb)==q->nextlineno_;
}
// size of q
size_t outq_size(struct outq_t*q){
  return priq_size(q->pq_);
}

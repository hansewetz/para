// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "inq.h"
#include "combuf.h"
#include "util.h"
#include "error.h"

// input queue constructor
struct inq_t*inq_ctor(int startlineno){
  struct inq_t*ret=emalloc(sizeof(struct inq_t));
  ret->nextlineno_=startlineno;
  ret->size_=0;
  ret->front_=NULL;
  ret->back_=NULL;
  return ret;
}
// input queue destructor
void inq_dtor(struct inq_t*q){
  while(!inq_empty(q)){
    struct combuf*cb=inq_front(q);
    inq_pop(q);
    combuf_dtor(cb);
  }
  free(q);
}
// get front of queue (might be NULL)
struct combuf*inq_front(struct inq_t*q){
  return q->front_;
}
// get back of queue (might be NULL)
struct combuf*inq_back(struct inq_t*q){
  return q->back_;
}
// push a combuf on input queue (line number will be automatically set in combuf)
void inq_push(struct inq_t*q,struct combuf*cb){
  combuf_setlineno(cb,q->nextlineno_++);
  if(q->front_==NULL){
    q->front_=q->back_=cb;
    cb->next_=NULL;
  }else{
    q->back_->next_=cb;
    q->back_=cb;
    cb->next_=NULL;
  }
  ++q->size_;
}
// pop front of queue (fatal if queue is empty)
void inq_pop(struct inq_t*q){
  if(q->front_==NULL)app_message(FATAL,"attempt to pop element of inq in empty queue in inq_pop()");
  struct combuf*cb=q->front_;
  q->front_=cb->next_;
  if(q->back_==cb)q->back_=NULL;
  cb->next_=NULL;
  --q->size_;
}
// next line number
int inq_nextlineno(struct inq_t*q){
  return q->nextlineno_;
}
// #of elements in queue
size_t inq_size(struct inq_t*q){
  return q->size_;
}
// 1 if queue is empty, else 0
int inq_empty(struct inq_t*q){
  return q->size_==0;
}
// true if input queu has combuf which is incomplete
int inq_partialrd(struct inq_t*q){
  if(inq_empty(q))return 0;
  struct combuf*cb=inq_back(q);
  return !combuf_rdcomplete(cb);
}
// true if input queue has data ready to be written
int inq_dataready(struct inq_t*q){
  if(inq_empty(q))return 0;
  struct combuf*cb=inq_front(q);
  return combuf_rdcomplete(cb);
}

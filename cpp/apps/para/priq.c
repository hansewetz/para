// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "priq.h"
#include "error.h"
#include "sys.h"
#include "util.h"

// --- helper functions ---

// swap objects
void swap(void**e1,void**e2){
  void*tmp=*e1;
  *e1=*e2;
  *e2=tmp;
}
// navigate up/down heap
static size_t hparent(size_t c){return (c-1)/2;}
static size_t hchild1(size_t p){return (p+1)*2-1;}
static size_t hchild2(size_t p){return (p+1)*2;}

// sink top element to its correct place
static void sinkel(struct priq*q){
  size_t p=0;
  while(1){
    if(p>=q->nel_)break;
    size_t c1=hchild1(p);
    size_t c2=hchild2(p);
    size_t c=c1;
    if(c>=q->nel_)break;
    if(c2<q->nel_){
      if(q->cmp_(q->vel_[c2],q->vel_[c1]))c=c2;
    }
    if(q->cmp_(q->vel_[p],q->vel_[c]))break;
    swap(&q->vel_[c],&q->vel_[p]);
    p=c;
  }
}
// float bottom element to its correct place
static void floatel(struct priq*q){
  if(q->nel_==0||q->nel_==1)return;
  size_t c=q->nel_-1;
  while(1){
    if(c==0)break;
    size_t p=hparent(c);
    if(q->cmp_(q->vel_[p],q->vel_[c]))break;
    swap(&q->vel_[p],&q->vel_[c]);
    c=p;
  }
}

// --- public functions ---

// constructor
struct priq*priq_ctor(size_t maxel,priq_cmp_t cmp){
  struct priq*q=emalloc(sizeof(struct priq));
  q->maxel_=maxel;
  q->nel_=0;
  q->cmp_=cmp;
  q->vel_=emalloc(maxel*sizeof(void*));
  return q;
}
// destructor
void priq_dtor(struct priq*q){
  free(q->vel_);
  free(q);
}
// print queue for debug purpose
void priq_dump(struct priq*q,priq_prnt_el pf){
  printf("maxel: %lu, nel: %lu\n",q->maxel_,q->nel_);
  printf("elements: ");
  for(size_t i=0;i<q->nel_;++i){
    pf(q->vel_[i]);
  }
}
// push an element on queue
void priq_push(struct priq*q,void*el){
  if(priq_full(q))app_message(FATAL,"priq overflow in priq_push()");
  q->vel_[q->nel_]=el;
  ++q->nel_;
  floatel(q);
}
// get top element
void*priq_top(struct priq*q){
  if(q->nel_==0)app_message(FATAL,"attempt to get top element of empty priq in priq_top()");
  return q->vel_[0];
}
// pop top element
void priq_pop(struct priq*q){
  if(q->nel_==0)app_message(FATAL,"attempt to pop top element of empty priq in priq_pop()");
  swap(&q->vel_[0],&q->vel_[q->nel_-1]);
  --q->nel_;
  sinkel(q);
}
// get #of elements in queue
size_t priq_size(struct priq*q){
  return q->nel_;
}
// check if we have reached maximum queue size
int priq_full(struct priq*q){
  return q->nel_>=q->maxel_;
}
// maximum size of queue
size_t priq_maxsize(struct priq*q){
  return q->maxel_;
}

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "priq.h"
#include "error.h"
#include "sys.h"
#include "util.h"
#include <string.h>

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
// float element to its correct place
// (if 'c' == -1 bottom element is floated, else element at index 'c' is floated)
static void floatel(struct priq*q,size_t c){
  if(q->nel_==0||q->nel_==1)return;
  if(c==-1)c=q->nel_-1;
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
struct priq*priq_ctor(size_t maxel,size_t inc,priq_cmp_t cmp){
  struct priq*q=emalloc(sizeof(struct priq));
  q->maxel_=maxel;
  q->inc_=inc;
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
  if(priq_full(q)){                  // check if we have eneough room to push element
    if(q->inc_==0)app_message(FATAL,"priq overflow in priq_push(), cannot extend queue since incremenet is zero (0)");
    size_t oldmaxel=q->maxel_;
    size_t newmaxel=oldmaxel+q->inc_;
    void**vel_old=q->vel_;
    app_message(WARNING,"priq overflow in priq_push(), extending priority queue from: %lu element to %lu elements",oldmaxel,newmaxel);
    q->maxel_=newmaxel;
    q->vel_=emalloc(newmaxel*sizeof(void*));
    memcpy(q->vel_,vel_old,oldmaxel*sizeof(q->vel_[0]));
  }
  q->vel_[q->nel_]=el;
  ++q->nel_;
  floatel(q,-1);
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
// remove an element from the queue
// ('el' must be an element in the queue)
void priq_remove(struct priq*q,void*el){
  // note: we need to find index of element before we can remove it - this is an O(N) operation
  size_t ind=-1;                                     // linear serach through heap
  for(size_t i=0;i<q->nel_&&ind==-1;++i){            // ...
    if(q->vel_[i]==el)ind=i;                         // ...
  }                                                  // ...
  if(ind==-1)app_message(FATAL,"attempt to remove element from priority queue not part of queue");
  swap(&q->vel_[ind],&q->vel_[q->nel_-1]);           // put last element in heap in deleted place
  floatel(q,ind);                                    // float it to correct place
  --q->nel_;                                         // last element no longer part of heap
}

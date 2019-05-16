// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>
#include <stdlib.h>

// --- output queue fuctionalities ---
// (wrapper around 'priq')

// output queue struct
struct outq_t{
  int nextlineno_;                                               // next line number to output (starting at 0)
  struct priq*pq_;                                               // priority queue
};

// basic methods
struct outq_t*outq_ctor(size_t maxel,int startlineno);           // output queue ctor
void outq_dtor(struct outq_t*q);                                 // output queue destructor
struct combuf*outq_front(struct outq_t*q);                       // get next combuf for output
void outq_push(struct outq_t*q,struct combuf*cb);                // push a combuf on queue
void outq_pop(struct outq_t*q);                                  // pop queue
int outq_ready(struct outq_t*q);                                 // true if top element on queue is ready (has correct line number) to be written
size_t outq_size(struct outq_t*q);                               // size of q

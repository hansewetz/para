// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>
#include <stdlib.h>

// --- input queue fuctionalities ---
// (plain simple FIFO queue)

// input queue struct
struct inq_t{
  int nextlineno_;                                 // next line number to output (starting at 0)
  size_t size_;                                    // #of elements in queue
  struct combuf*front_;                            // front (extraction point of elements) of queue
  struct combuf*back_;                             // back (insert point for new elements) of queue
};

// basic methods
struct inq_t*inq_ctor(int startlineno);            // input queue constructor
void inq_dtor(struct inq_t*q);                     // input queue destructor
struct combuf*inq_front(struct inq_t*q);           // get front of queue (might be NULL)
struct combuf*inq_back(struct inq_t*q);            // get back of queue (might be NULL)
void inq_push(struct inq_t*q,struct combuf*cb);    // push a combuf on input queue (line number will be automatically set in combuf)
void inq_pop(struct inq_t*q);                      // pop front of queue (fatal if queue is empty)
int inq_nextlineno(struct inq_t*q);                // next line number
size_t inq_size(struct inq_t*q);                   // #of elements in queue
int inq_empty(struct inq_t*q);                     // 1 if queue is empty, else 0
int inq_partialrd(struct inq_t*q);                 // does input queue have a buffer with partially read data
int inq_dataready(struct inq_t*q);                 // does input queue has data ready to be written

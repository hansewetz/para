// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>

// comparison function
// (first element < second element)
typedef int(*priq_cmp_t)(void*,void*);

// print element as a string to stream
typedef void(*priq_prnt_el)(void*);

// struct representing a priority queue
struct priq{
  size_t inc_;          // #of elements to incremenet queue with when overlow.
  size_t maxel_;        // max #of elements in priority queue (queue wil not be expanded)
  size_t nel_;          // #of elements currently in queue
  priq_cmp_t cmp_;      // comparison function for elements in queue
  void**vel_;           // array of pointers to elements
};
struct priq*priq_ctor(size_t maxel,size_t inc,priq_cmp_t cmp);  // constructor
void priq_dtor(struct priq*q);                                  // destructor
void priq_dump(struct priq*q,priq_prnt_el pf);                  // print queue for debug purpose
void priq_push(struct priq*q,void*el);                          // push an element on queue
void*priq_top(struct priq*q);                                   // get top element
void priq_pop(struct priq*q);                                   // pop top element
size_t priq_size(struct priq*q);                                // get #of elements in queue
int priq_full(struct priq*q);                                   // check if we have reached maximum queue size
size_t priq_maxsize(struct priq*q);                             // maximum size of queue
void priq_remove(struct priq*q,void*el);                        // remove an element from the queue ('el' must be an element in the queue)

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// pair struct
struct intpair{
  int first;
  int second;
};
void intpair_dump(struct intpair*p,FILE*fp,int nl);      // dump a pair on stream
void*emalloc(size_t size);                               // allocate memory and exit of error
int maxinfdsets(fd_set*rdset,fd_set*wrset);              // get max fd set in set
int maxulong(unsigned long x,unsigned long y);           // max of two unsigned longs
int maxint(int x,int y);                                 // max of two integers
char const*const bool2str(int v);                        // return 'true' or \fa;se'
int isposnumber(char const*s);                           // check if 's' is a positive number
FILE*efdopen(int fd,char const* mode);                   // open a FILE using an fd with error checking
void efpclose(FILE*fp);                                  // close an FILE*

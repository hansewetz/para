// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "util.h"
#include "error.h"
#include <string.h>
#include <ctype.h>
#include <errno.h>

// dump a pair on stream
void intpair_dump(struct intpair*p,FILE*fp,int nl){
  fprintf(fp,"first: %d, second: %d",p->first,p->second);
  if(nl)fprintf(stderr,"\n");
}
// allocate memory and exit if error
void*emalloc(size_t size){
  void*ret=malloc(size);
  if(ret==0)app_message(FATAL,"malloc failed");
  memset(ret,0,size);
  return ret;
}
// get max fd set in set
int maxinfdsets(fd_set*rdset,fd_set*wrset){
  int ret=-1;
  for(int i=0;i<FD_SETSIZE;++i){
    if(rdset&&FD_ISSET(i,rdset))ret=maxint(ret,i);
    if(wrset&&FD_ISSET(i,wrset))ret=maxint(ret,i);
  }
  return ret;
}
// max of two unsigned longs
int maxulong(unsigned long x,unsigned long y){
  return x>y?x:y;
}
// max of two integers
int maxint(int x,int y){
  return x>y?x:y;
}
// return 'true' or 'false'
char const*const bool2str(int v){
  static char const*const ret[]={"false","true"};
  return v==0?ret[0]:ret[1];
}
// check if 's' is a positive number
int isposnumber(char const*s){
  if(!s||!s[0])return 0;
  if(s[0]=='+'){
    if(!*++s)return 0;
  }
  for(;*s;++s){
    if(!isdigit(*s))return 0;
  }
  return 1;
}
// open a FILE using an fd with error checking
FILE*efdopen(int fd,char const* mode){
  FILE*fp=fdopen(fd,mode);
  if(!fp)app_message(FATAL,"failed opening fd: %d using fdopen()amode: %s, errno: %d, err: ",fd,mode,errno,strerror(errno));
  return fp;
}
// close an FILE*
void efpclose(FILE*fp){
  int stat=fclose(fp);
  if(stat==0)return;
  app_message(FATAL,"error while closeing file pointer, errno: %d, error: %s",errno,strerror(errno));
}

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// levels as strings
static char*LEVEL[]={"debug","info","warning","error","fatal"};

// cutoff level for errors
static enum errlevel_t cutoff_level=INFO;

// set cutoff error level
void loglevel(enum errlevel_t cutoff){
  cutoff_level=cutoff;
}
// print an application message and continue
void app_message(enum errlevel_t level,char const*msg,...){
  if(level>=cutoff_level){
    fprintf(stderr,"%s: ",LEVEL[(int)level]);
    va_list valist;
    va_start(valist,msg);
    vfprintf(stderr,msg,valist);
    va_end(valist);
    fprintf(stderr,"\n");
  }
  if(level==FATAL)exit(1);
}

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdlib.h>

// main loop in para
void paraloop(char const*cfile,char*cargv[],size_t nsubprocesses,size_t heart_sec,size_t maxoutq,size_t maxbuf,int startlineno,int fdin,int fdout);

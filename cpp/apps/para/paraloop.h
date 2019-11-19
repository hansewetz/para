// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdlib.h>

// main loop in para
// (loops around a 'pseleect()' system call)
void paraloop(char const*cfile,char*cargv[],size_t nsubprocesses,size_t client_tmo_sec,size_t heart_sec,size_t maxoutq,size_t incoutq,size_t maxbuf,int startlineno,int fdin,int fdout,size_t txncommitnlines,char const*txnlog,int recoveryenabled,int outIsPositionable);

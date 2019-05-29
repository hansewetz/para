// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include "util.h"
#include <stdlib.h>
#include <unistd.h>

// --- wrapper functions for system calls that should not fail ---
// (if a call fails the program is terminated with an error message)

void eclose(int fd);                                              // wrapper around close() system call
ssize_t ewrite(int fd,void const*buf,size_t count,int mustwrite); // write to fd with error checking
void setfdnonblock(int fd);                                       // set fd to non blocking mode
int edup(int fd);                                                 // dup with error checking
int ereadline(FILE*dp,char*buf,int bufmax,int mustread);          // read a line including NL or, until we reach EOF (return #of characters read - can be 0)
struct intpair spawn(char const*file,char*argv[]);                // spawn a child - return value [pid, fd] where pid is child pid, fd is fd to stdin/stdout for child
void ewaitpid(int pid);                                           // wait for a child process and handle errors
int eopen(char const*path,int oflag,mode_t mode_t);               // open a file, if error log and exit

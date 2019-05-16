// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>
#include <stdlib.h>

// --- combuf struct ---

// state of combuf
enum combuf_state{CBREAD=0,CBWRITE=1};

// state buffer tracking communication between reader and writer
struct combuf{
  int pid_;                 // -1 if combuf is not communication with a child process, else pid of child process
  FILE*fp_;                 // file pointer (fd retrieved with: filno(fp))
  struct tmo_t*tmo_;        // pointer to timeout - note: combuf does not own timer
  int lineno_;              // current line number
  enum combuf_state state_; // state of this buffer
  int eof_;                 // did we reach eof
  struct buf_t*buf_;        // buffer holding character and positions within buffer
  struct combuf*next_;      // so we can link combufs
};

// basic combuf methods
// (ctor private in c-file since all access to combuf objects shoulod go via a combuf_pool)
void combuf_dtor(struct combuf*cb);                                                     // destructor 
struct combuf*combuf_init(struct combuf*cb,FILE*fp,int lineno,enum combuf_state state); // initialize an existing combuf with new state (underlying buffer stays intact)
void combuf_dump(struct combuf*cb,FILE*fp,int nl);                                      // dump combuf to file for debug purposes
int combuf_pid(struct combuf*cb);                                                       // get pid for combuf
FILE*combuf_fp(struct combuf*cb);                                                       // get fp for combuf
int combuf_fd(struct combuf*cb);                                                        // get fd for combuf
int combuf_lineno(struct combuf*cb);                                                    // get lineno for combuf
enum combuf_state combuf_state(struct combuf*cb);                                       // get state of combuf
int combuf_eof(struct combuf*cb);                                                       // did we reach eof
struct buf_t*combuf_buf(struct combuf*cb);                                              // get character buffer in combuf
struct tmo_t*combuf_tmo(struct combuf*cb);                                              // get timer
void combuf_setpid(struct combuf*cb,int pid);                                           // set pid for combuf
void combuf_setlineno(struct combuf*cb,int lineno);                                     // set lineno in combuf (this is normally the only thing changing except buffer)
void combuf_settmo(struct combuf*cb,struct tmo_t*tmo);                                  // set timer in combuf
int combuf_empty(struct combuf*cb);                                                     // true if combuf is empty, else false

// combuf management methods
void combuf_swaprd4wr(struct combuf*rdsrc,struct combuf*wrtrg);                         // swap information from a source CDREAD combuf and update target to a CBWRITE combuf (destructive on rdsrc buf)
void combuf_clearwr2rd(struct combuf*cb);                                               // clear a CBWRITE combuf so we can read (keep line no and fp, clear tmo
void combuf_clear4rd(struct combuf*cb);                                                 // clear a CBREAD combuf so we can read again
void combuf_clear4wr(struct combuf*cb);                                                 // clear a CBWRITE combuf so we can write again

// combuf read/write methods
int combuf_rdcomplete(struct combuf*cb);                                                // does CBREAD combuf contain a complete line
int combuf_wrcomplete(struct combuf*cb);                                                // was entire line written from CBWRITE combuf
size_t combuf_read(struct combuf*cb,int seteof);                                        // read at most up to including LF
size_t combuf_write(struct combuf*cb,int seteof);                                       // write at most up to including LF

// --- combuf pool ---
// (pool of combuf structs)

struct combufpool{
  size_t maxbuf_;                                                                      // max size of buffers in pool
  struct combuf*head_;                                                                 // head of pool
};
struct combufpool*combufpool_ctor(size_t nel,enum combuf_state state,size_t maxbuf);   // contructor for pool of combufs
void combufpool_dtor(struct combufpool*p);                                             // pool destructor (wil kill all elements in pool)
struct combuf*combufpool_get(struct combufpool*p,FILE*fp,enum combuf_state state);     // get a combuf from pool, expand if needed
void combufpool_putback(struct combufpool*p,struct combuf*cb);                         // put back a combuf

// --- combuf table ---

// (we'll have one combuf for each subprocess we deal with)
struct combuftab{
  size_t size_;                                                                         // #of elements in table
  size_t allocated_;                                                                    // #of allocated elements
  struct combuf**tab_;                                                                  // pointers to eleemnts in table
};
struct combuftab*combuftab_ctor(size_t nel);                                            // create a fixed size combuf table - will own the elements in the table
void combuftab_dtor(struct combuftab*cbtab);                                            // destroy table - will destroy each if the elements also
size_t combuftab_size(struct combuftab*cbtab);                                          // get size of combuf table
void combuftab_add(struct combuftab*cbtab,struct combuf*cb);                            // add a combuf to combuf table - fails if no more room
struct combuf*combuftab_at(struct combuftab*cbtab,size_t ind);                          // get element at index 'ind' - might be NULL

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>
#include <stdlib.h>

// type of buffer
enum buftype{RDBUF=0,WRBUF};

// buffer used when reading data
struct buf_t{
  enum buftype type_;   // read or write buffer
  size_t maxbuf_;       // max character that can be written into buffer
  size_t nbuf_;         // #of character currently in buffer
  size_t ind_;          // index to next byte in buffer, for RDBUF == nbuf_, for WRBUF [0..nbuf_]
  char*buf_;            // buffer (will be allocated to contain maxbuf_+1 bytes, last byte for null termination)
};

// basic methods
struct buf_t*buf_ctor(enum buftype type,size_t maxbuf);     // constructor
void buf_dtor(struct buf_t*buf);                            // destructor
void buf_dump(struct buf_t*buf,FILE*fp,int nl);             // print buffer information
enum buftype buf_type(struct buf_t*buf);                    // get type of buffer (read/write)
size_t buf_maxbuf(struct buf_t*buf);                        // get max characters that buffer holds
size_t buf_nbuf(struct buf_t*buf);                          // get #of characters in buffer
int buf_empty(struct buf_t*buf);                            // return true if buffer empty, else false
size_t buf_ind(struct buf_t*buf);                           // get value of next byte to process in buffer
char*buf_buf(struct buf_t*buf);                             // get pointer to internal buffer
char*buf_bufrd(struct buf_t*buf);                           // get pointer to where to read data
char*buf_bufwr(struct buf_t*buf);                           // get pointer to where to start writing data from

// buffer management when quering/adding/consuming characters to/from buffer
char buf_lastchar(struct buf_t*buf);                        // return last character in buffer
size_t buf_nfree(struct buf_t*buf);                         // return #of bytes free in buffer
size_t buf_nconsume(struct buf_t*buf);                      // return #of bytes that can be consumed from buffer
void buf_reset(struct buf_t*buf,enum buftype type);         // reset buffer
void buf_rd2wr(struct buf_t*buf);                           // switch a RDBUF to a WRBUF (we have data in RDBUF and now wants to write it from an WRBUF)
void buf_add(struct buf_t*buf,size_t n);                    // update state after adding (reading in) characters to buffer
void buf_consume(struct buf_t*buf,size_t n);                // update state after consuming (writing out) characters from buffer

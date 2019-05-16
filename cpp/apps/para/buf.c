// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "buf.h"
#include "sys.h"
#include "error.h"
#include "util.h"

// constructor
struct buf_t*buf_ctor(enum buftype type,size_t maxbuf){
  struct buf_t*ret=emalloc(sizeof(struct buf_t));
  ret->type_=type;
  ret->maxbuf_=maxbuf;
  ret->nbuf_=0;
  ret->ind_=0;
  ret->buf_=emalloc(maxbuf+1);
  return ret;
}
// destructor
void buf_dtor(struct buf_t*buf){
  free(buf->buf_);
  free(buf);
}
// print buffer information
void buf_dump(struct buf_t*buf,FILE*fp,int nl){
  fprintf(fp,"type: %s, maxbuf: %lu, nbuf: %lu, ind: %ld",
          (buf->type_==RDBUF?"RDBUF":"WRBUF"),
          buf->maxbuf_,buf->nbuf_,(long int)buf->ind_);
  if(nl)fprintf(fp,"\n");
}
// get type of buffer (read/write)
enum buftype buf_type(struct buf_t*buf){
  return buf->type_;
}
// get max characters that buffer holds
size_t buf_maxbuf(struct buf_t*buf){
  return buf->maxbuf_;
}
// get #of characters in buffer
size_t buf_nbuf(struct buf_t*buf){
  return buf->nbuf_;
}
// return true if buffer empty, else false
int buf_empty(struct buf_t*buf){
  return buf_nbuf(buf)==0;
}
// get value of next byte to process in buffer
size_t buf_ind(struct buf_t*buf){
  return buf->ind_;
}
// get pointer to internal buffer
char*buf_buf(struct buf_t*buf){
  return buf->buf_;
}
// get pointer to where to read data
char*buf_bufrd(struct buf_t*buf){
  return &buf->buf_[buf->ind_];
}
// get pointer to where to start writing data from
char*buf_bufwr(struct buf_t*buf){
  return &buf->buf_[buf->ind_];
}
// return #of bytes free in buffer
size_t buf_nfree(struct buf_t*buf){
  return buf->maxbuf_-buf->nbuf_;
}
// return #of bytes that can be consumed from buffer
size_t buf_nconsume(struct buf_t*buf){
  return buf_nbuf(buf)-buf_ind(buf);
}
// return last character in buffer (terminate if buffer empty)
char buf_lastchar(struct buf_t*buf){
  if(buf_nbuf(buf)==0)app_message(FATAL,"attempt to retrieve last byte in empty buffer in buf_lastchar()");
  return buf->buf_[buf->ind_-1];
}
// reset buffer
void buf_reset(struct buf_t*buf,enum buftype type){
  buf->type_=type;
  buf->nbuf_=0;
  buf->ind_=0;
}
// switch a RDBUF to a WRBUF (we have data in RDBUF and now wants to write it from an WRBUF)
void buf_rd2wr(struct buf_t*buf){
  if(buf_type(buf)!=RDBUF)app_message(FATAL,"attempt to switch from an RDBUF to an WRBUF when buf type is not RDBUF in buf_rd2wr()");
  buf->type_=WRBUF;
  buf->ind_=0;
}
// update state after adding characters to buffer
void buf_add(struct buf_t*buf,size_t n){
  if(buf_type(buf)!=RDBUF)app_message(FATAL,"attempt to update buffer for read when buffer is not an RDBUF in buf_add()");
  if(buf_nfree(buf)<n)app_message(FATAL,"attempt to overflow buffer in buf_add()");
  buf->nbuf_+=n;
  buf->ind_+=n;
}
// update state after consuming characters from buffer
void buf_consume(struct buf_t*buf,size_t n){
  if(buf_type(buf)!=WRBUF)app_message(FATAL,"attempt to update buffer for write when buffer is not a WRBUF in buf_consume()");
  if(n>buf_nconsume(buf))app_message(FATAL,"attempt to consume too many bytes in buf_consume()");
  buf->ind_+=n;
}

// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "combuf.h"
#include "buf.h"
#include "sys.h"
#include "error.h"
#include "const.h"
#include "util.h"

// convert a state to a string
static char*state2string(enum combuf_state state){
  static char*st2str[]={"CBREAD","CBWRITE"};
  if(state==CBREAD)return st2str[0];
  return st2str[1];
}
// constructor
struct combuf*combuf_ctor(int pid,FILE*fp,int lineno,enum combuf_state state,size_t maxbuf){
  struct combuf*ret=emalloc(sizeof(struct combuf));
  ret->pid_=pid;
  ret->fp_=fp;
  ret->lineno_=lineno;
  ret->state_=state;
  ret->eof_=0;
  ret->buf_=buf_ctor(state==CBWRITE?WRBUF:RDBUF,maxbuf);
  ret->next_=NULL;
  return ret;
}
// destructor (flag specifying of buffer should be destroyed or not)
void combuf_dtor(struct combuf*cb){
  buf_dtor(cb->buf_);
  free(cb);
}
// initialize an existing piece of memory (similar to ctor)
struct combuf*combuf_init(struct combuf*cb,FILE*fp,int lineno,enum combuf_state state){
  cb->fp_=fp;
  cb->lineno_=lineno;
  cb->state_=state;
  return cb;
}
// dump combuf to file for debug purposes
void combuf_dump(struct combuf*cb,FILE*fp,int nl){
  fprintf(fp,"pid: %d,fd: %d, lineno: %d, state: %s, eof: %s, buf: [",
          cb->pid_,fileno(cb->fp_),cb->lineno_,state2string(cb->state_),(cb->eof_?"true":"false"));
  buf_dump(cb->buf_,fp,0);
  fprintf(fp,"]");
  if(nl)fprintf(fp,"\n");
}
// get pid for combuf
int combuf_pid(struct combuf*cb){
  return cb->pid_;
}
// get fp for combuf
FILE*combuf_fp(struct combuf*cb){
  return cb->fp_;
}
// get fd for combuf - if invalid return -1
int combuf_fd(struct combuf*cb){
  int fd=fileno(cb->fp_);
  return fd<0?-1:fd;
}
// get lineno for combuf
int combuf_lineno(struct combuf*cb){
  return cb->lineno_;
}
// get state of combuf
enum combuf_state combuf_state(struct combuf*cb){
  return cb->state_;
}
// did we reach eof
int combuf_eof(struct combuf*cb){
  return cb->eof_;
}
// get character buffer in combuf
struct buf_t*combuf_buf(struct combuf*cb){
  return cb->buf_;
}
// get timer
struct tmo_t*combuf_tmo(struct combuf*cb){
  return cb->tmo_;
}
// set pid in combuf
void combuf_setpid(struct combuf*cb,int pid){
  cb->pid_=pid;
}
// set lineno in combuf
void combuf_setlineno(struct combuf*cb,int lineno){
  cb->lineno_=lineno;
}
// set timer in combuf
void combuf_settmo(struct combuf*cb,struct tmo_t*tmo){
  cb->tmo_=tmo;
}
// true if combuf is empty, else false
int combuf_empty(struct combuf*cb){
  return buf_empty(cb->buf_);
}
// swap information from a source CDREAD combuf and update target to a CBWRITE combuf
// (trg must already be a CBWRITE combuf)
void combuf_swaprd4wr(struct combuf*rdsrc,struct combuf*wrtrg){
  if(combuf_state(rdsrc)!=CBREAD)app_message(FATAL,"attempt to swap for write from a CBWRITE combuf in combuf_swaprd4wr()");
  if(combuf_state(wrtrg)!=CBWRITE)app_message(FATAL,"attempt to swap for write to a CBREAD combuf in combuf_swaprd4wr()");

  // grab line number from rdsrc
  wrtrg->lineno_=rdsrc->lineno_;

  // swap internal buffers
  struct buf_t*tmp=rdsrc->buf_;
  rdsrc->buf_=wrtrg->buf_;
  wrtrg->buf_=tmp;

  // write buffer is the former read buffer --> switch buffer to write mode
  buf_rd2wr(wrtrg->buf_);

  // read buffer is the former write buffer --> reset buffer as a clean RDBUF
  buf_reset(rdsrc->buf_,RDBUF);
}
// clear a CBWRITE combuf so we can read as a CBREAD combuf (keep lineno and fp, clear tmo)
void combuf_clearwr2rd(struct combuf*cb){
  if(combuf_state(cb)!=CBWRITE)app_message(FATAL,"attempt to clear a CBREAD combuf to a CBWRITE combuf in combuf_clearwr2rd()");
  cb->state_=CBREAD;
  cb->tmo_=NULL;
  cb->eof_=0;
  buf_reset(cb->buf_,RDBUF);
}
// clear a combuf so we can read
void combuf_clear4rd(struct combuf*cb){
  cb->state_=CBREAD;
  cb->tmo_=NULL;
  cb->lineno_=0;
  cb->eof_=0;
  buf_reset(cb->buf_,RDBUF);
}
// clear a combuf so we can write
void combuf_clear4wr(struct combuf*cb){
  cb->state_=CBWRITE;
  cb->tmo_=NULL;
  cb->lineno_=0;
  cb->eof_=0;
  buf_reset(cb->buf_,WRBUF);
}
// does CBREAD combuf contain a complete line
// (if buffer is empty return false, else return 'lastchar==LF')
int combuf_rdcomplete(struct combuf*cb){
  if(combuf_state(cb)!=CBREAD)app_message(FATAL,"attempt to retrieve #of characters which can be read into CBWRITE combuf combuf_rdcomplete()");
  return buf_nbuf(cb->buf_)==0?0:buf_lastchar(cb->buf_)==LF;
}
// was entire line written from CBWRITE combuf
int combuf_wrcomplete(struct combuf*cb){
  if(combuf_state(cb)!=CBWRITE)app_message(FATAL,"attempt to retrieve #of characters to write from CBREAD combuf combuf_wrcomplete()");
  return buf_nconsume(cb->buf_)==0;
}
// read at most one line into buffer (including LF)
// (this is the only function we use when reading data into a combuf)
// (fp must be setup as non-blocking)
// (when calling this function we must 'know' there is data to read)
size_t combuf_read(struct combuf*cb,int seteof){
  if(combuf_state(cb)!=CBREAD)app_message(FATAL,"attempt to read characters into a CBWRITE combuf combuf_read()");
  if(combuf_eof(cb))app_message(FATAL,"attempt to read characters after reaching eof in combuf_read()");
  FILE*fp=combuf_fp(cb);                    // get fp to read from
  struct buf_t*buf=combuf_buf(cb);          // get buffer to read into
  size_t max2read=buf_nfree(buf);           // max #of characters we can add to buffer
  if(max2read==0)app_message(FATAL,"attempt to read into full buffer in combuf_read()");
  int nread=ereadline(fp,buf_bufrd(buf),max2read,seteof);
  if(nread>0)buf_add(buf,nread);            // do book keeping in buffer (update indices)
  if(nread==0){                             // we reached eof
    if(seteof)cb->eof_=1;                   // set eof marker in combuf
    if(buf_nbuf(buf)>0&&buf_lastchar(buf)!=LF){// we might be missing a LF at eof - if so add it
      if(buf_nfree(buf)==0)app_message(FATAL,"cannot append LF to buffer after reaching EOF - not eneough room in buffer in combuf_read()");
      *buf_bufrd(buf)=LF;                   // add LF character
      buf_add(buf,1);                       // do book keeping for adding LF character
      ++nread;
    }
  }else
  if(buf_nbuf(buf)>0&&buf_lastchar(buf)!=LF&&buf_nfree(buf)==0){// last character is not a LF and we have no more room
    app_message(FATAL,"no room in buffer for adding a LF in buffer in combuf_read()");
  }
  return nread;
}
// write at most up to including LF
// (this is the only function we use when writing data out from a combuf)
size_t combuf_write(struct combuf*cb,int seteof){
  if(combuf_state(cb)!=CBWRITE)app_message(FATAL,"attempt to write characters out of a CBREAD combuf combuf_write()");
  if(combuf_eof(cb))app_message(FATAL,"attempt to write characters after reaching eof in combuf_write()");
  FILE*fp=combuf_fp(cb);                    // get fp to write to
  struct buf_t*buf=combuf_buf(cb);          // get buffer to write from
  size_t max2write=buf_nconsume(buf);       // max #of characters we can write out of buffer
  if(max2write==0)app_message(FATAL,"attempt to write from buffer that has no characters to write in combuf_write()");
  int nwritten=ewrite(fileno(fp),buf_bufwr(buf),max2write,seteof);
  if(nwritten>0)buf_consume(buf,nwritten);  // do book keeping in buffer (update indices)
  if(nwritten==0){                          // we hit eof
    if(seteof)cb->eof_=1;                   // set eof marker in combuf
  }
  if(cb->eof_&&buf_nconsume(buf)>0){        // if we hit eof and there are still characters left in buffer there is an error
    app_message(FATAL,"failed to write all characters in buffer before fd closed in combuf_write()");
  }
  return nwritten;
}

// --- combuf pool ---

// contructor for pool of combufs
struct combufpool*combufpool_ctor(size_t nel,enum combuf_state state,size_t maxbuf){
  struct combufpool*ret=emalloc(sizeof(struct combufpool));
  ret->maxbuf_=maxbuf;
  ret->head_=0;
  for(size_t i=0;i<nel;++i){
    combufpool_putback(ret,combuf_ctor(-1,0,0,state,maxbuf));
  }
  return ret;
}
// pool destructor (will kill all elements in pool)
void combufpool_dtor(struct combufpool*p){
  struct combuf*cb=p->head_;
  while(cb){
    struct combuf*cbnext=cb->next_;
    combuf_dtor(cb);
    cb=cbnext;
  }
  free(p);
}
// get a combuf from pool, create new one if needed
struct combuf*combufpool_get(struct combufpool*p,FILE*fp,enum combuf_state state){
  if(p->head_){
    struct combuf*ret=p->head_;
    p->head_=ret->next_;
    ret->next_=NULL;
    ret->fp_=fp;
    ret->state_=state;
    ret->eof_=0;
    return ret;
  }
  return combuf_ctor(-1,fp,0,state,p->maxbuf_);
}
// put back a combuf
void combufpool_putback(struct combufpool*p,struct combuf*cb){
  cb->next_=p->head_;
  p->head_=cb;

}

// --- combuf table ---

// create a fixed size combuf table - will own the elements in the table
struct combuftab*combuftab_ctor(size_t nel){
  struct combuftab*ret=emalloc(sizeof(struct combuftab));
  ret->size_=0;
  ret->allocated_=nel;
  ret->tab_=emalloc(ret->allocated_*sizeof(struct combuf));
  return ret;
}
// destroy table - will destroy each if the elements
void combuftab_dtor(struct combuftab*cbtab){
  for(size_t i=0;i<cbtab->size_;++i){
    if(cbtab->tab_[i])combuf_dtor(cbtab->tab_[i]);
  }
  free(cbtab->tab_);
  free(cbtab);
}
// get size of combuf table
size_t combuftab_size(struct combuftab*cbtab){
  return cbtab->size_;
}
// add a combuf to combuf table - fails if no more room
void combuftab_add(struct combuftab*cbtab,struct combuf*cb){
  for(size_t i=0;i<cbtab->allocated_;++i){
    if(cbtab->tab_[i]!=0)continue;
    cbtab->tab_[i]=cb;
    ++cbtab->size_;
    return;
  }
  app_message(FATAL,"attempt to add a combuf to full combuftab table in combuftab_add()");
}
// get eleemnt at index 'ind' - might be NULL
struct combuf*combuftab_at(struct combuftab*cbtab,size_t ind){
  if(ind>=cbtab->allocated_)app_message(FATAL,"attempt to index outside range in combuftab_byind()");
  return cbtab->tab_[ind];
}

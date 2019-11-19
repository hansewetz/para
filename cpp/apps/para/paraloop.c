// (C) Copyright Hans Ewetz 2019. All rights reserved.

/* TODO
  - have an eof for output instead of checking output combuf all the time
  - maybe remove linenumber in combuf ctor and always set it to -1 if not specified
  - try to read many line simultaneously when reading input
*/
#include "error.h"
#include "priq.h"
#include "tmo.h"
#include "buf.h"
#include "combuf.h"
#include "outq.h"
#include "inq.h"
#include "sys.h"
#include "txn.h"
#include "util.h"
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// helper methods
static int readinq(struct inq_t*qin,FILE*fpin,size_t maxlines,struct combufpool*cbpool);                         // read lines from input
static int flushoutq(struct outq_t*qout,struct combufpool*cbpool,size_t txncommitnlines,struct txn_t*txn,struct txnlog_t*lasttxnlog,struct txnlog_t*nexttxnlog);// flush output queue
static void inq2cbtab(struct inq_t*qin,struct combuf*cb,fd_set*wrall_set,struct combufpool*cbpool);              // transfer data from inq to child process write buffer
static int cbtabread(struct combuf*cb,fd_set*rdall_set,fd_set*fdrd);                                             // read data into sub process buffer
static int cbtabwrite(struct combuf*cb,fd_set*rdall_set,fd_set*wrall_set,fd_set*wrset,struct combufpool*cbpool); // write data in sub process buffer
static void cbtab2outq(struct outq_t*qout,struct combuf*cb,fd_set*wrall_set,struct combufpool*cbpool,FILE*fpout);// copy sub process buffer to output queue
static void handle_txn(size_t txncommitnlines,struct txnlog_t*lasttxnlog,struct txnlog_t*nexttxnlog,int forcecommit,struct txn_t*txn); // commit transaction if needed

// handle SIGCHLD signal
static int childExited=0;
void sigchldHandler(int signo){
  app_message(WARNING,"sigchldHandler caught sigchild");
  int pid;
  int stat;
  while((pid=waitpid(-1,&stat,WNOHANG))>0||(stat<0&&errno==EINTR)){
    if(pid>0){
      app_message(WARNING,"child with pid: %d terminated",pid);
      childExited=1;
    }
  }
}
// retrieve recovery info - if any
void recoveryinfo(char const*txnlogfile,size_t*skipnfirstlines,size_t*skipoutputpos){
  struct txn_t*rtxn=txn_ctor(-1,txnlogfile);      // create a transaction for recovery, we won't use output fd in transaction
  struct txnlog_t*rtxnlog=txn_recover(rtxn);      // retrieve recovered txn log
  if(rtxnlog!=0){
    *skipnfirstlines=txnlog_nlines(rtxnlog);       // #of lines to skip
    *skipoutputpos=txnlog_outfilepos(rtxnlog);     // file position in output file
    txn_setKeeplog(rtxn,1);                       // destroy temporary transaction but don't touch transaction log
    txn_dtor(rtxn);                               // ...
  }
}
// select loop
// (this is the main loop in the para program)
void paraloop(char const*cfile,char*cargv[],size_t nsubprocesses,size_t client_tmo_sec,size_t heart_sec,size_t maxoutq,size_t outqinc,size_t maxbuf,int startlineno,int fdin,int fdout,size_t txncommitnlines,char const*txnlogfile,int recoveryenabled,int outIsPositionable){
  // set input and output to non-blocking
  setfdnonblock(fdin);
  setfdnonblock(fdout);

  // if recovery is enabled then retrieve #of lins that were committed
  // (note: we can do recovery even if we won't execute in transactional mode)
  size_t skipnfirstlines=0;                         // #of lines to skip when starting
  size_t skipoutputpos=0;                           // skip to filepos in output file
  if(recoveryenabled){
    recoveryinfo(txnlogfile,&skipnfirstlines,&skipoutputpos);
    app_message(INFO,"skipping first: %d lines in recovery mode, outfilepos: %lu ...",skipnfirstlines,skipoutputpos);

    // if we can position within output then go ahead and do it
    if(outIsPositionable&&skipoutputpos>0){
      app_message(INFO,"positioning to offset: %lu in output stream",skipoutputpos);
      int stat=lseek(fdout,skipoutputpos,SEEK_SET);
      if(stat<0)app_message(FATAL,"failed moving position in output file during recovery to offset: %lu",skipoutputpos);
    }
  }
  // setup fd sets for select()
  fd_set rdall_set,wrall_set;                       // read set for input adn write for output
  FD_ZERO(&rdall_set);                              // clear all in both sets
  FD_ZERO(&wrall_set);                              // ...
  FD_SET(fdin,&rdall_set);                          // set read fd - everything is started by reading from input

  // setup timer queue
  size_t maxtmos=1+nsubprocesses;                   // maxtmos: heartbeat timer + one timer for each child process
  struct priq*qtmo=tmoq_ctor(maxtmos);
  struct tmo_t*heart_tmo=tmo_ctor(HEARTBEAT,heart_sec,-1);
  tmoq_push(qtmo,heart_tmo);

  // setup input queue
  // (input queue is a linear FIFO queue)
  int inputeof=0;
  struct inq_t*qin=inq_ctor(startlineno);

  // setup output queue
  // (output queue is a priority queue with lowest line number at front)
  int outputeof=0;
  struct outq_t*qout=outq_ctor(maxoutq,outqinc,startlineno+skipnfirstlines);

  // setup a pool of free combuf objects
  // (combufpool is a free pool of combuffers)
  size_t cbpool_init_size=1+maxoutq+2*nsubprocesses;                  // #of combufs = maxoutq + maxinq + nsubprocesses+spare(inqread())
  struct combufpool*cbpool=combufpool_ctor(cbpool_init_size,CBWRITE,maxbuf);

  // track positional info in order to handle commits
  struct txnlog_t*lasttxnlog=txnlog_ctor(skipnfirstlines,skipoutputpos);
  struct txnlog_t*nexttxnlog=txnlog_ctor(skipnfirstlines,skipoutputpos);
  struct txn_t*txn=0;                                                 // transaction
  if(txncommitnlines>0)txn=txn_ctor(fdout,txnlogfile);                // check if we need to configure transaction

  // setup signal handler for SIGCHLD
  struct sigaction sigact;                                            // setup signal handler for SIGCHLD
  memset(&sigact,0,sizeof sigact);                                    // ...
  sigact.sa_handler=sigchldHandler;                                   // ...
  if(sigaction(SIGCHLD,&sigact,0)<0){                                 // ...
    app_message(FATAL,"failed in sigaction(): %s",strerror(errno));   // bail out
  }
  sigset_t mask,orig_mask;                                            // block SIGCHLD outside select()
  sigemptyset(&mask);                                                 // ...
  sigaddset(&mask,SIGCHLD);                                           // ...
  if(sigprocmask(SIG_BLOCK,&mask,&orig_mask)<0){                      // ...
    app_message(FATAL,"failed in sigprocmask(): %s",strerror(errno)); // bail out
  }
  // setup a table of combufs for tracking child processes
  // (combuftab is a table with entries tracking sub processes - each element is a simple combuf struct)
  struct combuftab*cbtab=combuftab_ctor(nsubprocesses);
  for(size_t i=0;i<nsubprocesses;++i){
    struct intpair p=spawn(cfile,cargv);
    FILE*fp=efdopen(p.second,"rwb");
    struct combuf*cb=combufpool_get(cbpool,fp,CBWRITE);
    combuf_setpid(cb,p.first);
    combuftab_add(cbtab,cb);
  }
  // setup a table mapping fd --> FILE* 
  // (we use this table to map fd's to FILE pointers to use when reading/writing)
  // (at the IO level it's up to the IO routines to choose between FILE* and fd's)
  // (for reading line it is simpler to use 'fgets()' instead of managing IO buffering our selves)
  int fd2fpmap_size=maxint(fdin,fdout);
  for(size_t i=0;i<nsubprocesses;++i){
    struct combuf*cb=combuftab_at(cbtab,i);
    fd2fpmap_size=maxint(fd2fpmap_size,combuf_fd(cb));
  }
  fd2fpmap_size+=1;
  FILE**fd2fpmap=emalloc(fd2fpmap_size*sizeof(FILE**));

  // add entries into 'fd2fpmap'
  fd2fpmap[fdin]=efdopen(fdin,"rb");
  fd2fpmap[fdout]=efdopen(fdout,"wb");
  for(size_t i=0;i<nsubprocesses;++i){
    struct combuf*cb=combuftab_at(cbtab,i);
    fd2fpmap[combuf_fd(cb)]=combuf_fp(cb);
  }
  // loop until nothing more to read/write ...
  while(1){                                                      // loop until we are not waiting for read or write anymore
    fd_set rdset=rdall_set;                                      // grab current fd masks (read and write)
    fd_set wrset=wrall_set;                                      // ...
    int maxfd=maxinfdsets(&rdset,&wrset);                        // get max fd
    struct timespec tspec;                                       // get timeout
    sigset_t emptyset;                                           // signal mask to pass to pselect()
    sigemptyset(&emptyset);                                      // ...
    struct timespec*ptspec=tmoq_select_timeout(qtmo,&tspec);     // ... (returns null if timer queue is empty)
    int sstat=pselect(maxfd+1,&rdset,&wrset,0,ptspec,&emptyset); // do pselect() call ...

    // check if we received a SIGCHLD signal
    // (can happen if exec() call fails)
    if(childExited)app_message(FATAL,"child process exited");

    // select() error
    if(sstat<0)app_message(FATAL,"maxfd: %d, tv: %lu, %lu",maxfd,ptspec->tv_sec,ptspec->tv_nsec);

    // select() timeout
    if(sstat==0){
      struct tmo_t*tmo=tmoq_front(qtmo);                       // get popped timer and remove it from timer queue
      if(tmo==NULL)app_message(FATAL,"timer popped but no timer on tmo queue in para.cc");
      app_message(INFO,"timer popped, type: %s",tmo_type2str(tmo));
      tmoq_pop(qtmo);                                          // remove timer from queue
      if(tmo_type(tmo)==HEARTBEAT){                            // did we get a heartbeat or a child timeout tmo?
        tmoq_push(qtmo,tmo_reactivate(tmo));                   // reactivate heartbeat timer
      }else{                                                   // a child timedout ... we'll terminate since no point continuing
        size_t ind=tmo_key(tmo);                               // get combuf for child process that timed out
        struct combuf*cb=combuftab_at(cbtab,ind);              // ...
        app_message(FATAL,"child process timeout for pid: %d ... terminating",combuf_pid(cb));
      }
    }
    // (1) read data into input queue (select triggered on input fd)
    if(FD_ISSET(fdin,&rdset)){
      if(!inputeof)inputeof=readinq(qin,fd2fpmap[fdin],nsubprocesses,cbpool);
    }
    // (1.5) if we 'skipnfirstlines>0 remove 'skipnfirstlines' from input queue
    if(skipnfirstlines>0){
      skipnfirstlines-=inq_popnlines(qin,skipnfirstlines);
    }
    // (2) copy data from input queue into sub-process buffer
    for(size_t i=0;i<nsubprocesses;++i){
      struct combuf*cb=combuftab_at(cbtab,i);                    // get combuf for sub-process and check if we can write data to it
      if(combuf_state(cb)!=CBWRITE)continue;                     // not a WRITE buffer
      inq2cbtab(qin,cb,&wrall_set,cbpool);                       // transfer data from inq to child process if possible
    }
    // (3) write data stored in child process buffer + set timer for chile process if needed
    for(size_t i=0;i<nsubprocesses;++i){
      struct combuf*cb=combuftab_at(cbtab,i);                    // get combuf for sub-process and check if we can write data to it
      if(combuf_state(cb)!=CBWRITE)continue;                     // not a WRITE buffer
      int complete=cbtabwrite(cb,&rdall_set,&wrall_set,&wrset,cbpool);// write data stored in child process buffer
      if(complete){                                              // if we wrote a complete buffer, then set child timer
        struct tmo_t*client_tmo=tmo_ctor(CLIENT,client_tmo_sec,i);// the 'key' for timer is the index into 'cbtab'
        combuf_settmo(combuftab_at(cbtab,i),client_tmo);          // set tmo in combuf fro client process so that we can retrieve it ;ater
        tmoq_push(qtmo,client_tmo);                               // push timer on tmo queue
      }
    }
    // (4) read data into child process buffer + remove timer from child process if needed
    for(size_t i=0;i<nsubprocesses;++i){
      struct combuf*cb=combuftab_at(cbtab,i);                    // get combuf for sub-process and check if we can write data to it
      if(combuf_state(cb)!=CBREAD)continue;                      // not a READ buffer
      int complete=cbtabread(cb,&rdall_set,&rdset);              // read data into child process buffer
      if(complete){                                              // if we read a complete buffer then remove child timer
        struct combuf*cb=combuftab_at(cbtab,i);                  // deactivate client timer 
        struct tmo_t*client_tmo=combuf_tmo(cb);                  // ...
        tmoq_remove(qtmo,client_tmo);                            // remove timer from queue
        tmo_dtor(client_tmo);                                    // destroy timer
      }
    }
    // (5) copy data from sub process buffer to output queue
    for(size_t i=0;i<nsubprocesses;++i){
      struct combuf*cb=combuftab_at(cbtab,i);                    // get combuf for sub-process and check if we can write data to it
      if(combuf_state(cb)!=CBREAD)continue;                      // not a READ buffer
      cbtab2outq(qout,cb,&wrall_set,cbpool,fd2fpmap[fdout]);     // copy data from sub process buffer to output queue
    }
    // (6) flush output queue (select() triggered on output fd)
    if(FD_ISSET(fdout,&wrset)){
      if(!outputeof)outputeof=flushoutq(qout,cbpool,txncommitnlines,txn,lasttxnlog,nexttxnlog);
    }
    // trigger on input in select()?
    // (do not trigger if eof, trigger if there is partially read data or, if we have fewer than 'nsubprocesses' lines in input queue)
    if(!inputeof&&(inq_partialrd(qin)||inq_size(qin)<nsubprocesses)){
      FD_SET(fdin,&rdall_set);
    }
    else{
      FD_CLR(fdin,&rdall_set);
    }
    // trigger on output in select()?
    if(outq_ready(qout)){
      FD_SET(fdout,&wrall_set);
    }
    else{
      FD_CLR(fdout,&wrall_set);
    }
    // done?
    if(maxinfdsets(&rdall_set,&wrall_set)<0&&inq_size(qin)==0&&outq_size(qout)==0){
      break;
    }
  }
  app_message(DEBUG,"#timers in queue: %d (expected: 1 HEARTBEAT timer)",qtmo->nel_);

  // we can do a final commit at this point
  // (txn will flush output file before committing)
  handle_txn(txncommitnlines,lasttxnlog,nexttxnlog,1,txn);
  if(txn){                             // only if we have a transaction ...
    txn_setKeeplog(txn,0);             // make sure transaction log is removed in tx destructor
    txn_dtor(txn);                     // destroy transaction object
  }
  txnlog_dtor(lasttxnlog);             // destroy transaction log object
  txnlog_dtor(nexttxnlog);             // destroy transaction log object

  // close all FILE* in fd2fpmap
  app_message(DEBUG,"closing files ...");
  for(int i=0;i<fd2fpmap_size;++i){
    FILE*fp=fd2fpmap[i];
    if(!fp)continue;
    efpclose(fp);
  }
  // wait for all child processes to terminate
  // (all pid's in combufs in the combuf table are valid)
  app_message(DEBUG,"waiting for child processes ...");
  for(size_t i=0;i<combuftab_size(cbtab);++i){
    struct combuf*cb=combuftab_at(cbtab,i);
    ewaitpid(combuf_pid(cb));
  }
  // cleanup allocated memory
  app_message(DEBUG,"cleaning up memory ...");
  free(fd2fpmap);                                                // free memory for table mapping fd --> FILE*
  combuftab_dtor(cbtab);                                         // destroy child process table
  tmoq_dtor(qtmo);                                               // cleanup time queue
  outq_dtor(qout);                                               // output queue
  inq_dtor(qin);                                                 // input queue
  combufpool_dtor(cbpool);                                       // pool of combufs
  app_message(DEBUG,"... cleanup done");
}
// read a line from input and store in input queue
// (we must know we can read since if we read 0 bytes we'll interpret it as if we reached eof)
// (returns 1 if eof reached, else false)
int readinq(struct inq_t*qin,FILE*fpin,size_t maxlines,struct combufpool*cbpool){
  int firsttime=1;
  struct combuf*cbin=0;                                       // buffer we are dealing with
  int isfreebuf=0;                                            // keep track of if the buffer we are dealing with is 'free' (not in queue) or in queue
  while(inq_size(qin)<maxlines){                              // keep a fixed number of lines loaded in input queue
    isfreebuf=0;
    cbin=inq_back(qin);                                       // get last element in queue
    if(cbin==NULL||combuf_rdcomplete(cbin)){                  // get a new buffer if qin is empty or, last element in qin is complete
      isfreebuf=1;                                            // remember that the buffer is a 'free buffer', not in the queue
      cbin=combufpool_get(cbpool,fpin,CBREAD);                // get hold of a new combuf
      combuf_clear4rd(cbin);                                  // initialize from scratch
    }
    size_t nread=combuf_read(cbin,firsttime);                 // read unless we reached eof
    if(nread==0)break;                                        // if we read 0 bytes we are done
    if(combuf_eof(cbin))break;                                // return if we reached eof
    if(!combuf_rdcomplete(cbin))break;                        // if we didn't read a complete line, no point continuing
    if(isfreebuf){                                            // if we have a free buffer we need to push it onto queue
      inq_push(qin,cbin);                                     // ...
      isfreebuf=0;                                            // ...
    }
    firsttime=0;
  }
  if(!combuf_empty(cbin)&&combuf_eof(cbin)&&!combuf_rdcomplete(cbin)){ // if we reached eof with a non-empty buffer that is not complete we have an error
    app_message(FATAL,"reached input EOF before reading a complete line from input");
  }
  if(combuf_empty(cbin)){
    if(isfreebuf){
      combufpool_putback(cbpool,cbin);                          // if we didn't read anything - putback buffer in free pool
    }
  }else{
    if(isfreebuf)inq_push(qin,cbin);                            // add buffer to inq if we have data in buffer
  }
  return combuf_eof(cbin);
}
// flush output queue as much as we can
// (we stop when qout is closed (eof), qout is empty or we cannot write more data to output
// (returns 1 if eof reached, else false)
int flushoutq(struct outq_t*qout,struct combufpool*cbpool,size_t txncommitnlines,struct txn_t*txn,struct txnlog_t*lasttxnlog,struct txnlog_t*nexttxnlog){
  int firsttime=1;                                           // must track if first time, since writing 0 bytes first time means eof
  while(outq_ready(qout)){                                   // as long as we have a buffer with right line number ...
    struct combuf*cbout=outq_front(qout);                    // get top of queue (we'll only have one entry in queue for right now)
    size_t nwritten=0;
    if(!combuf_eof(cbout))nwritten=combuf_write(cbout,firsttime);// write unless we reached eof on output
    if(combuf_wrcomplete(cbout)){                            // if buffer is completly written pop it from queue
      outq_pop(qout);                                        // ...
      combufpool_putback(cbpool,cbout);                      // ...
      ++nexttxnlog->nlines_;                                 // increment #of full lines written
      nexttxnlog->outfilepos_+=buf_size(combuf_buf(cbout));  // increment variable tracking position in output file
      handle_txn(txncommitnlines,lasttxnlog,nexttxnlog,0,txn); // check if we need to commit transaction
    }
    if(combuf_eof(cbout))return 1;                           // only the flag eof only if this is the first timeiun the loop since the first time we MUST be able tro write
    if(nwritten==0)break;                                    // done - nothing written (we must be second or third ... time around
    firsttime=0;                                             // ...
  }
  return 0;
}
// transfer data from inq to child process if possible
void inq2cbtab(struct inq_t*qin,struct combuf*cb,fd_set*wrall_set,struct combufpool*cbpool){
  if(!combuf_empty(cb))return;                              // if buffer is not empty then we are busy doing something with it
  int fd=combuf_fd(cb);                                     // get fd to child process
  if(!inq_dataready(qin))return;                            // if no data to get from qin, nothing to do
  struct combuf*cbin=inq_front(qin);                        // get buffer from input queue
  combuf_swaprd4wr(cbin,cb);                                // swap internal input/output buffers
  inq_pop(qin);                                             // we are done with cbin from input queue
  combufpool_putback(cbpool,cbin);                          // put cbin back in pool
  FD_SET(fd,wrall_set);                                     // trigger on write next time around
}
// write data waiting in child process combuf
// (return true if complete buffer was written, else false)
int cbtabwrite(struct combuf*cb,fd_set*rdall_set,fd_set*wrall_set,fd_set*wrset,struct combufpool*cbpool){
  int fd=combuf_fd(cb);                                     // get fd to child process
  if(!FD_ISSET(fd,wrset))return 0;                          // if we cannot write then nothing to do
  combuf_write(cb,1);                                       // we now have buffer for child process - write as much as possibly
  if(!combuf_wrcomplete(cb))return 0;                       // we did not write complete buffer - will continue next time around
  combuf_clearwr2rd(cb);                                    // if we wrote complete buffer, then switch combuf to read mode
  FD_SET(fd,rdall_set);                                     // prepare to read from child process (trigger on read in select())
  FD_CLR(fd,wrall_set);                                     // we are done writing to child process - clear write select() flag
  return 1;
}
// read as much data as possible into child process buffer
// (we do not transfer it to output queue yet)
// (return true if we read a complete buffer, else return false)
int cbtabread(struct combuf*cb,fd_set*rdall_set,fd_set*rdset){
  if(combuf_rdcomplete(cb))return 0;                          // if buffer is complete then nothing to do here
  int fd=combuf_fd(cb);                                       // get fd to child process
  if(!FD_ISSET(fd,rdset))return 0;                            // if we cannot read then nothing to do here
  combuf_read(cb,1);                                          // read as much as possible
  if(!combuf_rdcomplete(cb))return 0;                         // return if we didn't read a complete line from child
  FD_CLR(fd,rdall_set);                                       // if we completed buffer, turn off read flag
  return 1;
}
// copy data from child process buffer to output queue
void cbtab2outq(struct outq_t*qout,struct combuf*cb,fd_set*wrall_set,struct combufpool*cbpool,FILE*fpout){
  if(!combuf_rdcomplete(cb))return;                         // if buffer is not complete nothing to do here
  struct combuf*cbout=combufpool_get(cbpool,fpout,CBWRITE); // get a combuf for writing
  combuf_swaprd4wr(cb,cbout);                               // swap child process read buffer with buffer to be added to output queue
  outq_push(qout,cbout);                                    // push it on output queue
  combuf_clear4wr(cb);                                      // clear child process buffer so we can write to it
}
// commit transaction
static void handle_txn(size_t txncommitnlines,struct txnlog_t*lasttxnlog,struct txnlog_t*nexttxnlog,int forcecommit,struct txn_t*txn){
  if(!txn)return;                                                                 // check if txn is enabled
  if(txnlog_nlines(lasttxnlog)==txnlog_nlines(nexttxnlog))return;                 // no need to commit if we committed at this point earlier
  if(forcecommit||(nexttxnlog->nlines_%txncommitnlines)==0){                      // commit if forced or if we reached commit point
    txn_commit(txn,nexttxnlog);                                                   // commit 'outlines' #of lines
    app_message(INFO,"committed at %lu lines ...",nexttxnlog->nlines_);           // log so we know at what line we committed
    txnlog_setnlines(lasttxnlog,txnlog_nlines(nexttxnlog));                       // update 'lasttxnlog' object
    txnlog_setoutfilepos(lasttxnlog,txnlog_outfilepos(nexttxnlog));               // ...
  }
}

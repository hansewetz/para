// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "txn.h"
#include "sys.h"
#include "error.h"
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// --- transcation log struct ---

// ctor
struct txnlog_t*txnlog_ctor(size_t nlines,size_t outfilepos){
  struct txnlog_t*ret=emalloc(sizeof(struct txnlog_t));
  ret->nlines_=nlines;
  ret->outfilepos_=outfilepos;
  return ret;
}
// dtor
void txnlog_dtor(struct txnlog_t*txnlog){
  free(txnlog);
}
// getters
size_t txnlog_nlines(struct txnlog_t*txnlog){return txnlog->nlines_;}
size_t txnlog_outfilepos(struct txnlog_t*txnlog){return txnlog->outfilepos_;}

// setters
void txnlog_setnlines(struct txnlog_t*txnlog,size_t nlines){txnlog->nlines_=nlines;}
void txnlog_setoutfilepos(struct txnlog_t*txnlog,size_t outfilepos){txnlog->outfilepos_=outfilepos;}

// debug print function for transaction log
void txnlog_dump(struct txnlog_t*txnlog,FILE*fp,int nl){
  fprintf(fp,"nlines: %lu, outfilepos: %lu",txnlog->nlines_,txnlog->outfilepos_);
  if(nl)fprintf(fp,"\n");
}

// --- transcation struct ---

/*
 * NOTE! should maintain open fds for transaction log so we don't have to close/open it all the time
*/

// constructor
struct txn_t*txn_ctor(int fdout,int cansyncoutfd_,char const*txnlogfile){
  struct txn_t*ret=emalloc(sizeof(struct txn_t));           // create txn object
  char*tmpext=".tmp";                                       // extension of temp transaction log
  size_t maxtxnlog=FILENAME_MAX-strlen(tmpext);             // max length of transaction log
  if(strlen(txnlogfile)>maxtxnlog)app_message(FATAL,"'txnlog' filename too long, maximimum length is: %lu",FILENAME_MAX-4);
  sprintf(ret->tmptxnlogfile_,"%s%s",txnlogfile,tmpext);    // setup temporary transaction log
  strcpy(ret->txnlogfile_,txnlogfile);                      // copy txn log filename to internal structue
  ret->keeplog_=1;                                          // keep transaction log in destructor
  ret->fdout_=fdout;                                        // save fd to output file
  ret->cansyncoutfd_=cansyncoutfd_;                         // can we sync fd?
  char tmplogfilename1[FILENAME_MAX-1];                     // get directory in which we write transaction log - it needs to be flushed after we flush transaction log
  strcpy(tmplogfilename1,txnlogfile);                       // ... 
  char txnlogdir[FILENAME_MAX-1];                           // name of directory containing transaction log
  strcpy(txnlogdir,dirname(tmplogfilename1));               // ...
  ret->fdtxnlogdir_=eopen(txnlogdir,O_DIRECTORY,0777);      // open directory containing transaction log - we'll need to flush it at commit time
  return ret;                                               // return transaction object
}
// destructor
void txn_dtor(struct txn_t*txn){
  if(!txn->keeplog_)eunlink(txn->txnlogfile_);              // unlink txn log
  eclose(txn->fdtxnlogdir_);                                // close directory containing transaction log
}
// set flag specifying if txn log should be removed in destructor
void txn_setKeeplog(struct txn_t*txn,int keeplog){
  txn->keeplog_=keeplog;
}
// commit transaction
void txn_commit(struct txn_t*txn,struct txnlog_t*txnlog){
  if(txn->cansyncoutfd_)efsync(txn->fdout_);                                 // sync output file to disk
  int fdtmplog=eopen(txn->txnlogfile_,O_WRONLY|O_CREAT|O_TRUNC,0777);        // open file for temporary transaction log

  // write txn log
  int stat1;
  stat1=write(fdtmplog,(char*)&txnlog->nlines_,sizeof(size_t));                // write #of bytes
  if(stat1<0)app_message(FATAL,"write to temporary transaction log failed (nlines), errno: %d, errstr: %s",errno,strerror(errno));
  stat1=write(fdtmplog,(char*)&txnlog->outfilepos_,sizeof(size_t));            // write #of bytes
  if(stat1<0)app_message(FATAL,"write to temporary transaction log failed (outfilepos), errno: %d, errstr: %s",errno,strerror(errno));

  // sync and commit
  efsync(fdtmplog);                                    // sync log to disk
  eclose(fdtmplog);                                    // close temp txn log
  efsync(txn->fdtxnlogdir_);                           // sync directory cotaining transaction log
  int stat2=rename(txn->txnlogfile_,txn->txnlogfile_); // ATOMICALLY rename temporary transaction log to the real transaction log
  if(stat2<0)app_message(FATAL,"rename of temporary transcation log failed, errno: %d, errstr: %s",errno,strerror(errno));
}
// recover transaction
// (returns #of lines committed, if txn log does not exist return value is 0)
struct txnlog_t*txn_recover(struct txn_t*txn){
  // check if file exist + open it
  if(access(txn->txnlogfile_,R_OK)!=0)return 0;             // check if file exist
  int fdlog=eopen(txn->txnlogfile_,O_RDONLY,0777);          // open transaction log for reading

  // allocate transaction log
  struct txnlog_t*ret=emalloc(sizeof(struct txnlog_t));     // allocate transaction log object

  // read saved transaction log
  int stat;
  stat=read(fdlog,(char*)&ret->nlines_,sizeof(size_t));                 // ...
  if(stat!=sizeof(ret))app_message(FATAL,"failed reading recovery information from transaction log (nlines), errno: %d, errstr: %s",errno,strerror(errno));
  stat=read(fdlog,(char*)&ret->outfilepos_,sizeof(size_t));              // ...
  if(stat!=sizeof(ret))app_message(FATAL,"failed reading recovery information from transaction log (outfilepos_), errno: %d, errstr: %s",errno,strerror(errno));

  // we are done ... close transaction log and return object
  eclose(fdlog);
  return ret;                                               // return transaction log object
}
// dump information about transaction on a file
void txn_dump(struct txn_t*txn,FILE*fp,int nl){
  fprintf(fp,"tmptxnlogfile: %s, tmptxnlog: %s, keeplog: %s",txn->txnlogfile_,txn->tmptxnlogfile_,txn->txnlogfile_?"true":"false");
  if(nl)fprintf(fp,"\n");
}

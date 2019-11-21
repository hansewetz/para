// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once
#include <stdio.h>

// --- structure implementing transactions for para ---

// transaction log
struct txnlog_t{
  size_t nlines_;                                                   // #of lines at commit point
  size_t outfilepos_;                                               // file position in output file
};
// ctor, dtor
struct txnlog_t*txnlog_ctor(size_t nlines,size_t outfilepos);       // transaction log constructor
void txnlog_dtor(struct txnlog_t*txnlog);                           // transaction log constructor

// methods
size_t txnlog_nlines(struct txnlog_t*txnlog);                       // getter
size_t txnlog_outfilepos(struct txnlog_t*txnlog);                   // ...
void txnlog_setnlines(struct txnlog_t*txnlog,size_t nlines);        // setter
void txnlog_setoutfilepos(struct txnlog_t*txnlog,size_t outfilepos);// setter
void txnlog_dump(struct txnlog_t*txn,FILE*fp,int nl);               // print transaction log information

// transaction class
struct txn_t{
  char txnlogfile_[FILENAME_MAX+1];                                 // name of transaction-log file
  char tmptxnlogfile_[FILENAME_MAX+1];                              // name of transaction-log file
  int fdout_;                                                       // fd for output file we are committing for
  int cansyncoutfd_;                                                // true if we can sync fd
  int keeplog_;                                                     // true: do not remove txn log in destructor, false: remove log in destructor
};
// ctor, dtor
struct txn_t*txn_ctor(int fdout,int cansyncoutfd,char const*txnlogfile);// constructor (parameters: fd to file we are managing, name of transaction log)
void txn_dtor(struct txn_t*txn);                                    // destructor

// transaction related methods
void txn_commit(struct txn_t*txn,struct txnlog_t*txnlog);           // commit transaction at 'nlines'
void txn_setKeeplog(struct txn_t*txn,int keeplog);                  // set flag if log should be kept or remove in destructor
struct txnlog_t*txn_recover(struct txn_t*txn);                      // recover (if needed) from 
void txn_dump(struct txn_t*txn,FILE*fp,int nl);                     // print transaction information

// (C) Copyright Hans Ewetz 2019. All rights reserved.

/* TODO

  - possibly have a timeout when itmes are sitting in the output queue too long blocking progress
    possibly also add a maximum size of output queue even when an incremenet for the queue is specified

  - write more than one line to a sub-process -- most probably we are not efficient in IO the way it is currently done

  - add more diagnostic debug messages in paraloop.c

  - draw a diagram showing how all data structures fits together --> add to README.md file on github

  - make allocation of tmo's more efficient - create and copy by value

  - better scheme to remove timers from timer queue - right now it's an O(N+logN) operation
    (we can as well re-build the heap in linear time)

  - add dump of stats at end of processing
    (internal details, stats about #of lines, timings etc.)

  - support input/output as tcp connectios

  - support 'child process' being a tcp service

*/

#include "version.h"
#include "paraloop.h"
#include "error.h"
#include "util.h"
#include "sys.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <fcntl.h>

// max #of cmd line parameters to child process
#define ARG_MAX 1024

// command line parameters
static int print=0;                                // print cmd line parameters (default false)
static int verbose=0;                              // verbose level (maximum debug level on)
static int version=0;                              // print version number and exit (default false)
static int printrecoveryinfo=0;                    // print recovery info
static size_t maxclients=1;                        // #of child processes
static size_t clientsec=5;                         // client tmo in seconds
static size_t heartsec=5;                          // heart beat timer sec
static size_t maxoutq=1000;                        // max size of output priority queue (a child process might be slow blocking output) 
static size_t incoutq=1000;                        // increment when extending output queue
static size_t maxbuf=4096;                         // max length of a line in bytes
static int startlineno=0;                          // line number for first line
static char*cmd=NULL;                              // command to execute in child processes
static char*inputfile=NULL;                        // inputfile, if NULL the <stdin>
static char*outputfile=NULL;                       // outputfile, if NULL the <stdout>
static size_t txncommitnlines=0;                   // commit every 'txncommitnlines' - if 0, no commits are executed
static char*cargv[ARG_MAX+1];                      // command line arguments for child processes

// other global variable
static char*txnlog=".para.txnlog";                 // default name of transaction lg
static size_t txnenabled=0;                        // transactional mode enabled
static size_t recoveryenabled=0;                   // recovery mode enabled

// usage strings
static char*strusage[]={
  "Usage:",
  "  para [options] -- maxclients cmd cmdargs ...",
  "",
  "options:",
  "  -h          help and exit (default: not set)",
  "  -p          print cmd line parameters (default: not set)",
  "  -v          verbose mode (maximum debug level, default: not set)",
  "  -V          print version number (optional, default: not set)",
  "  -r          print recovery info - if any - and exit",
  "  -R          execute in recovery mode (default: no recovery is performed , optional)",
  "  -b arg      maximum length in bytes of a line (optional, default: 4096)",
  "  -T arg      timeout in seconds waiting for response from a sub-process (default 5)",
  "  -H arg      heartbeat in seconds (optional, default: 5)",
  "  -m arg      #of child processes to spawn (optional if specified as a positional parameter, default: 1)",
  "  -M arg      maximum number of lines to store in the output queue before terminating, if non-zero the queue will be incremented (see '-x' option) (default: 1000)",
  "  -x arg      increment output queue with this #of elements if overflow (default: 1000)",
  "  -c arg      command to execute in child process (optional if specified as positional parameter)",
  "  -i arg      input file (default is standard input, optional)",
  "  -o arg      output file (default is standard output, optional)",
  "  -C arg      execute in transactional mode with #of lines per commit (default: no commits are performed , optional)",
  "  --",
  "  maxclients  #of child processes to spawn (optional if specified as command line parameter, default: 1)",
  "  cmd         command to execute in child processes (optional if specified as '-c' option)",
  "  cmdargs     arguments to 'cmd' sub-command)",
  NULL
};
// print cmd line parameters
void printcmds(){
  fprintf(stderr,"----------------------------\n");
  fprintf(stderr,"-p: %s\n",bool2str(print));
  fprintf(stderr,"-v: %s\n",bool2str(verbose));
  fprintf(stderr,"-V: %s\n",bool2str(version));
  fprintf(stderr,"-r: %s\n",bool2str(printrecoveryinfo));
  fprintf(stderr,"-R: %s\n",bool2str(recoveryenabled));
  fprintf(stderr,"-b: %lu\n",maxbuf);
  fprintf(stderr,"-T: %lu\n",clientsec);
  fprintf(stderr,"-H: %lu\n",heartsec);
  fprintf(stderr,"-m: %lu\n",maxclients);
  fprintf(stderr,"-M: %lu\n",maxoutq);
  fprintf(stderr,"-I: %lu\n",incoutq);
  fprintf(stderr,"-c: %s\n",cmd);
  fprintf(stderr,"-i: %s\n",inputfile?inputfile:"<stdin>");
  fprintf(stderr,"-o: %s\n",outputfile?outputfile:"<stdout>");
  fprintf(stderr,"-C: %lu\n",txncommitnlines);
  fprintf(stderr,"----------------------------\n");
}
// print version
void printversion(){
  fprintf(stderr,"@version: %d.%d\n",PARA_VERSION_MAJOR,PARA_VERSION_MINOR);
  exit(0);
}
// usage function - print message to stderr and exit
// print an application message and continue
void usage(char const*msg,...){
  va_list valist;
  va_start(valist,msg);
  vfprintf(stderr,msg,valist);
  va_end(valist);
  for(char**p=strusage;*p;++p){
    fprintf(stderr,"%s\n",*p);
  }
  exit(1);
}
// main test program
int main(int argc,char**argv){
  int opt;
  while((opt=getopt(argc,argv,"hpvVrRC:T:H:b:m:M:x:c:i:o:"))!=-1){ // get non-positional command line parameters
    switch(opt){
    case 'h':
      usage("");
    case 'p':
      print=1;
      break;
    case 'v':
      verbose=1;
      break;
    case 'V':
      version=1;
      break;
    case 'r':
      printrecoveryinfo=1;
      break;
    case 'R':
      recoveryenabled=1;
      break;
    case 'b':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-b' option, must be a positive number",optarg);
      if((maxbuf=atol(optarg))<2)usage("parameter to '-b' must be a positive number greater than two (2)");
      break;
    case 'C':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-C' option, must be a positive number",optarg);
      if((txncommitnlines=atol(optarg))<1)usage("parameter to '-C' must be a positive number greater or equal to than zero");
      txnenabled=1;
      break;
    case 'T':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-T' option, must be a positive number",optarg);
      if((clientsec=atol(optarg))<1)usage("parameter to '-T' must be a positive number greater than zero");
      break;
    case 'H':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-H' option, must be a positive number",optarg);
      if((heartsec=atol(optarg))<1)usage("parameter to '-H' must be a positive number greater than zero");
      break;
    case 'm':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-m' option, must be a positive number",optarg);
      maxclients=atol(optarg);
      break;
    case 'M':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-M' option, must be a positive number",optarg);
      maxoutq=atol(optarg);
      break;
    case 'x':
      if(!isposnumber(optarg))usage("invalid parameter '%s' to '-x' option, must be a positive number",optarg);
      incoutq=atol(optarg);
      break;
    case 'c':
      cmd=optarg;
      break;
    case 'i':
      inputfile=optarg;
      break;
    case 'o':
      outputfile=optarg;
      break;
    case '?':
      usage("unknown option: %c\n",optopt);
    }
  }
  if(version)printversion();                                                   // print version of para if requested
  if(printrecoveryinfo){                                                       // check if we should print recovery info
    size_t skipnfirstlines=0;
    size_t skipoutputpos=0;
    recoveryinfo(txnlog,&skipnfirstlines,&skipoutputpos);
    fprintf(stderr,"recovery info --> #lines-committed: %lu, outfile-pos: %lu\n",skipnfirstlines,skipoutputpos);
    exit(0);
  }
  int pospar=0;                                                                // get positional parameters
  int cargc=0;                                                                 // ...
  for(;optind<argc;++optind,++pospar){                                         // ...
    if(cargc>=ARG_MAX)app_message(FATAL,"too many command line parameters to child process, max: %d",ARG_MAX);
    char const*optarg=argv[optind];                                            // ...
    if(pospar==0){                                                             // 'maxclient'
      if(cmd)usage("'maxclients' cannot be specified both as a command line parameters ('m') and as a positional argument ('maxclients')");
      if(!isposnumber(optarg))usage("invalid parameter '%s' as first positional argument, 'maxclient' must be a positive number",optarg);
      maxclients=atol(optarg);                                                 // ...
    }else                                                                      // ...
    if(pospar==1){                                                             // 'cmd'
      if(cmd)usage("'cmd' cannot be specified both as a command line parameters ('c') and as a positional argument ('cmd')");
      cmd=argv[optind];                                                        // ...
      cargv[cargc++]=cmd;                                                      // ...
    }else{                                                                     // 'cmdargs'
      cargv[cargc++]=argv[optind];                                             // ...
    }
  }
  cargv[cargc++]=NULL;                                                         // need to terminate cmd parameters for child processes

  // check that we have all parameters
  if(!cmd)usage("'cmd' (or -c) command line parameters must specify command for child process");

  if(verbose)loglevel(DEBUG);                                                  // set debug level
  if(print)printcmds();                                                        // print cmd linet parameters if needed

  // open input file if needed
  int fdin=STDIN_FILENO;                                                       // input and output fds
  int fdout=STDOUT_FILENO;                                                     // ...
  if(inputfile)fdin=eopen(inputfile,O_RDONLY,0777);                            // open input file for reading

  // open output differently depending on how recovery flag is set and if transaction log exists
  if(outputfile){                                                              // if output file is specified then open it with correct parameters
    int txnlogexists=0;                                                        // flag set to true of transactio log exist 
    if(access(txnlog,R_OK)==0)txnlogexists=1;                                  // check if transaction log exists and can be access in read mode
    int oflags=O_WRONLY;                                                       // open outfile in write only mode
    if(recoveryenabled&&txnlogexists){                                         // we'll try to recover
      // no extra flags to set since we'll do an lseek(...) in the file        // append to output file
    }else{                                                                     // no recovery
      oflags|=O_CREAT|O_TRUNC;                                                 // truncate outfile if no recovery
    }                                                                          // ...
    if(outputfile)fdout=eopen(outputfile,oflags,0777);                         // open output file for writing
  }
  // kickoff select() loop
  paraloop(cmd,cargv,maxclients,clientsec,heartsec,maxoutq,incoutq,maxbuf,startlineno,fdin,fdout,txncommitnlines,txnlog,
           recoveryenabled,outputfile!=0,outputfile!=0);
}

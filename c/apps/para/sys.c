// (C) Copyright Hans Ewetz 2019. All rights reserved.
#include "error.h"
#include "sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/prctl.h>

// wrapper around close() system call
void eclose(int fd){
  int stat;
  while((stat=close(fd))<0&&errno==EINTR);
  if(stat<0)app_message(FATAL,"eclose: close failed: %s",strerror(errno));
}
// write to fd with error checking
ssize_t ewrite(int fd,void const*buf,size_t count,int mustwrite){
  int wstat;
  // 'fd' is set to non-blocking mode and we will call this function in two cases:
  //   - 'select()' indicates that it is possible to write to 'fd' or that 'fd' closed, in this case 'mustwrite' is set
  //     in this case we generate a fatal error if write would block
  //   - we have already written because 'select()' indicated we could, and we continue and try to write
  //     in this case we return '0' if we get an error indicating that write would block
  // The man page for write(2) states:
  //     write(3) man: If O_NONBLOCK is set and the STREAM cannot accept data, write() shall return -1 and set errno to [EAGAIN]
  //                   If O_NONBLOCK is set and part of the buffer has been written while a condition in which the STREAM cannot 
  //                    accept additional data occurs, write() shall terminate and return the number of  bytes written
  // Which indicates that if 'select()' has triggered on write on 'fd' and 'fd' is non-blocking we should never receive a 'EAGAIN' from write()
  // This is because select() should not trigger unless we can write
  // The problem is that when writing to a slow device such as a terminal (or more precisely a pseudo terminal) we do get the error EAGAIN.
  // The solution here is to try to write again when errno == EAGAIN - i.e., effectivly doing a blocking write
  while((wstat=write(fd,buf,count))<0){
    if(errno==EINTR)continue;                  // interupted by signal - try again
    if(errno==EAGAIN&&!mustwrite)return 0;     // we would block, but we don't have to write anything
    if(errno==EAGAIN)break;                    // we would block, but since 'mustwrite' is set we should be able to write (i.e., it should never happen)
  }
  if(wstat<0)app_message(FATAL,"error writing in ewrite(): %s, errno: %d, nbytes: %lu, buf: %s",strerror(errno),errno,count,buf);
  return wstat;
}
// set fd to non blocking mode
void setfdnonblock(int fd){
  int flags=fcntl(fd,F_GETFL,0);
  if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
    app_message(FATAL,"setFdNonblock: failed setting fd in non-blocking mode: %s",strerror(errno));
  }
}
// dup with error checking
int edup(int fd){
  int dstat;
  while((dstat=dup(fd))<0&&errno==EINTR);
  if(dstat<0)app_message(FATAL,"error reading in edup(): %s",strerror(errno));
  return dstat;
}
// read a line including LF or, until we reach EOF
// (return #of characters read - can be 0)
int ereadline(FILE*fp,char*buf,int bufmax,int mustread){
/*
#define MAXFDS 1024
  static FILE*fptab[MAXFDS];
  if(fd>=MAXFDS)app_message(FATAL,"cannot support file descriptor with value higher than: %d",MAXFDS);
  FILE*fp=fptab[fd];
  if(fp==0){
    fp=fptab[fd]=fdopen(fd,"rb");
    if(!fp)app_message(FATAL,"failed opening fd: %d using fdopen(), errno: %d, err: ",fd,errno,strerror(errno));
  }
*/
  char*ret=fgets(buf,bufmax,fp);
  if(errno==EAGAIN&&!mustread)return 0;
  if(ret==0){
    if(errno<0)app_message(FATAL,"failed reading line, errno: %d, errstr: ",errno,strerror(errno));
    return 0;
  }
  return strlen(buf);
}
// spawn a child - return value [pid, fd] where pid is child pid, fd is fd to stdin/stdout for child
// (arguments are the same as for execvp())
struct intpair spawn(char const*file,char*argv[]){
  int ppid=getpid();                 // get pid of future parent (this process)
  int fds[2];
  int stat=socketpair(AF_LOCAL,SOCK_STREAM,0,fds);
  if(stat<0)app_message(FATAL,"socketpair failed, errno: %d, errstr: %s in spawn()",errno,strerror(errno));
  int pid=fork();
  if(pid<0)app_message(FATAL,"fork failed, errno: %d, errstr: %s in spawn()",errno,strerror(errno));
  if(pid>0){                         // parent - use fds[0]
    eclose(fds[1]);                  // close child side - we don't need it
    setfdnonblock(fds[0]);           // make sure fd is non-blocking on parent side
    struct intpair pret={pid,fds[0]};// parent side of full duplex pipe
    return pret;
  }else{                             // child - use fds[1]
    if(prctl(PR_SET_PDEATHSIG,SIGTERM)<0){ // make sure we die when parent dies
      app_message(FATAL,"failed prctl call,errno: %d, errstr: %s  in spawn()",errno,strerror(errno));
    }
    if(ppid!=getppid()){             // make sure parent pid has not changed - i.e., parent died
      app_message(FATAL,"child detected that parent process died ... shutting down child");
    }
    eclose(fds[0]);                  // close parent side of full duplex pipe
    eclose(0);                       // dup stdin and stdout --> full duplex pipe
    eclose(1);                       // ...
    edup(fds[1]);                    // ...
    edup(fds[1]);                    // ...
    eclose(fds[1]);                  // ...

    // execute child process
    if(execvp(file,argv)<0){
      app_message(FATAL,"child failed executing execvp, errno: %d, errstr: %s in spawn()",errno,strerror(errno));
    }else{
      // should never get here
      app_message(FATAL,"child failed executing execvp, errno: %d, errstr: %s in spawn",errno,strerror(errno));
    }
  }
  // we'll never get here
  __builtin_unreachable();
}
// wait for a child process and handle errors
void ewaitpid(int pid){
  int stat;
  int pret;
  while((pret=waitpid(pid,&stat,0))<0&&errno==EINTR);
  if(pret==pid){
    // check how child process terminated
    if(WIFEXITED(stat)){
      int exitcode=WEXITSTATUS(stat);
      if(exitcode!=0)app_message(WARNING,"child process with pid: %d terminated with non-zero exit code: %d",pid,exitcode);
    }else
    if(WIFSIGNALED(stat)){
/* WCOREDUMP not always defined
      if(WCOREDUMP(stat)){
        int signal=WTERMSIG(stat);
        app_message(WARNING,"child process with pid: %d core dumped from signal: %d",pid,signal);
      }else{
*/
        // we ignore warning if signal is SIGPIPE since we did close the pipe to the child process casing it to terminate
        int signal=WTERMSIG(stat);
        if(signal!=SIGPIPE)app_message(WARNING,"child process with pid: %d terminated from signal: %d",pid,signal);
/*
      }
*/
    }else{
      app_message(WARNING,"child process with pid: %d terminated for unknown reason",pid);
    }
  }else
  if(pret==0){
    app_message(WARNING,"child process with pid: %d did not terminate",pid);
  }else
  if(pret<0){
    app_message(WARNING,"failed waiting for child process with pid: %d to terminate, errno: %d, errstr: %d",pid,errno,strerror(errno));
  }
}
// open a file, if error log and exit
int eopen(char const*path,int oflag,mode_t mode){
  int stat=open(path,oflag,mode);
  if(stat<0)app_message(FATAL,",failed opening file: %s, errno: %d, errstr: %s",path,errno,strerror(errno));
  return stat;
}
// sync fd to disk
void efsync(int fd){
  int stat=fsync(fd);
  if(stat<0)app_message(FATAL,"failed syncing file descriptor (fsync), errno: %d, errstr: %s",errno,strerror(errno));
}
// seek in file
size_t elseek(int fd,size_t offset,int whence){
  int stat=lseek(fd,offset,whence);
  if(stat<0)app_message(FATAL,"failed lseek, errno: %d, errstr: %s",errno,strerror(errno));
  return stat;
}
// unlink a file
void eunlink(const char *path){
  int stat=unlink(path);
  if(stat<0)app_message(FATAL,"failed unlink, errno: %d, errstr: %s",errno,strerror(errno));
}

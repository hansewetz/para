# ```para``` <sub><sup>(version 0.3)</sup></sub>

```para``` is a program for efficient parallel processing of linebased text. 
```para``` delegates processing of each line in a file to a user specified sub command which is managed and executed by ```para```. ```para``` collects the output from sub-commands and prints the output in the same order had the text been processed serially from the input file.

# Installing ```para```

Install ```para``` by following the steps:
```
$ mkdir para && cd para
$ git clone https://github.com/hansewetz/para.git .
$ mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX:PATH=<my-install-dir>  .. && make && make install
$ export PATH=${PATH}:<my-install-dir>/bin
```

We can now  execute:

```$> para -- 1 cat
$ para -- 1 cat                          # run with a single sub-process
Hello                                    # your text
Hello                                    # echoed by para
info: timer popped, type: HEARTBEAT      # default heartbeat is every 5 sec
info: timer popped, type: HEARTBEAT      # ...
^D                                       # stop with ^D
```
# A simple example

Say we want to calculate the ```md5sum``` for each line in a file using a script stored in ```md5.bash```:

```
#!/bin/bash
while read line; do
  echo $line | md5sum | awk '{print $1}'
done
```

The following command:

```
$ echo hello | ./md5.bash
```

will print:

```
b1946ac92492d2347c6235b4d2611184
```

Now lets calculate the ```md5sum``` for each line in a medium size file having around 500K lines:

```
$ time zcat monolingual.en.gz | ./md5.bash > out1.md5sum
```

The execution time is slightly more than 20 minutes:

```
real    21m22.874s
user    10m18.485s
sys     28m48.447s
```

Now we'll use ```para``` to speed up the processing:

```
$ time zcat monolingual.en.gz | para -- 5 ./md5.bash > out2.md5sum
```

The processing time is now:

```
real    4m34.788s
user    10m42.282s
sys     29m59.132s
```

To make sure ```para``` behaves correctly we'll make sure the output from the ```para``` command generates identical output as the first command executed without ```para```:

```
$ diff out1.md5sum out2.md5sum | wc -l
0
```

In our example ```para``` starts 5 sub processes running the ```md5.bash``` command. 

We can see that the speedup is approximately 5 times. Linear speedup is not always possible though. When the sub-command does not do much heavy calculations the overhead of ```para``` takes over and the ```para``` solution might perform worse.

# ```para``` command line parameters

```para``` is a written and designed as a standard UNIX like utility. It takes named command line parameters as well as a few positional parameters. By default it  reads from ```stdin``` and ```stdout```. For backwards compatibility and historical reasons few positional command line parameters are supported. Running ```	para -h``` produces:

```
$ Usage:
  para [options] -- maxclients cmd cmdargs ...

options:
  -h          help and exit (default: not set)
  -p          print cmd line parameters (default: not set)
  -v          verbose mode (maximum debug level, default: not set)
  -V          print version number (optional, default: not set)
  -r          print recovery info - if any - and exit
  -R          execute in recovery mode (default: no recovery is performed , optional)
  -b arg      maximum length in bytes of a line (optional, default: 4096)
  -T arg      timeout in seconds waiting for response from a sub-process (default 5)
  -H arg      heartbeat in seconds (optional, default: 5)
  -m arg      #of child processes to spawn (optional if specified as a positional parameter, default: 1)
  -M arg      maximum number of lines to store in the output queue before terminating, if non-zero the queue will be incremented (see '-x' option) (default: 1000)
  -x arg      increment output queue with this #of elements if overflow (default: 1000)
  -c arg      command to execute in child process (optional if specified as positional parameter)
  -i arg      input file (default is standard input, optional)
  -o arg      output file (default is standard output, optional)
  -C arg      execute in transactional mode with #of lines per commit (default: no commits are performed , optional)
  --
  maxclients  #of child processes to spawn (optional if specified as command line parameter, default: 1)
  cmd         command to execute in child processes (optional if specified as '-c' option)
  cmdargs     arguments to 'cmd' sub-command)
```
```para``` takes a relatively small number of command line parameters. Some parameters control basic functional behavior whereas other are used for tweaking ```para``` at the technical level. Most parameters related to technical issues have reasonable default values and should not have to be specified.

## timeout while waiting for sub-process

After ```para``` sends a line to a sub-process it waits for a response. If the response time is longer than a specified timeout value (default is 5 seconds) ```para``` terminates all sub-processes with an error message and exits with a non-zero error code.

For example, if we force client sub-processes to take 2 seconds to respond and while setting the ```para``` timeout to 1 second as in the following command:

```
$ echo 'hello' | para -T 1 -- 5 unbuffer -p awk '{system("sleep 2");print $1;}'
```

we will receive a timeout error:

```
info: timer popped, type: CLIENT
fatal: child process timeout for pid: 28599 ... terminating
```

## heartbeat timer

```para``` supports a *heartbeat* timer so that it is possible to see that ```para``` is alive even when processing input is received sporadically. For example:

```
# run para with a 2 second heartbeat (-H 2) echoing input to output
$ while :; do sleep 1; echo hello; done | ./cpp/apps/para/para -H 2 -- 5 cat
```

will generate output similar to:

```
hello
info: timer popped, type: HEARTBEAT
hello
hello
info: timer popped, type: HEARTBEAT
hello
hello
info: timer popped, type: HEARTBEAT
hello
^C
```

## input and output files

By default input is read from```stdin``` and output is written to ```stdout```. If one or both of the command line parameters ```-i inputfile``` and ```-o outputfile``` are specified, input is read from the ```inputfile``` and output is written to the ```outputfile```.

Currently only file input and output is supported in addition to ```stdin``` and ```stdout```. Future extension will support reading and writing across network connections.

## internal limits in ```para```

If a single specific line in the input file takes a long time to be processed by the sub command, the specific line will block the output queue. Until the slow-processing-line has bee generated the output queue will stay blocked since output must be written in the same order as input was read in.

By default the output queue is extended automatically by the increment specified by the ```-x``` command line parameter (default value: 1000). If ```-x``` specifies an increment of ```0``` the queue will not be extended and ```para``` will terminate when the output queue is full.


# Running sub-commands that buffer data

Sub-commands that buffers input and/or output presents a problem to ```para```. This because ```para``` operates on a line by line basis waiting for a response from a sub-process before it will send a new request to the sub-process. A sub-process buffering output will effectivly block ```para``` from feeding sub-processes with new requests. 

This section will show you have to manage ```sed```, ```awk``` and other programs that buffer data.

## ```sed``` as a sub-command

 ```sed``` can be run with the flag ```--unbufferd``` when executed as a sub-command to ```para```:

```
$ echo 'hello again' | para -- 5 sed --unbuffered 's/a/z/g'
```

prints:

```
hello zgzin
```

Without the ```--unbuffered``` flag ```sed``` will simply hang while waiting in a ```read``` system call that tries to read a large chunk of data from ```stdin```.

## a general solution

A more general solution to buffering is to use the ``unbuffer`` command:

```
$ echo 'hello again' | para -- 5 unbuffer -p sed 's/a/z/g'
```

```unbuffer``` runs ```sed``` ensuring that input and output into and from ```sed``` is not buffered.

Running ```awk```as a sub-command presents similar buffering problems that also can be solved using ```unbuffer```. For example, reversing the order of words on each line can be done with the command:

```
$ para -- 10 unbuffer -p gawk '{for(i=NF;i>1;i--) printf("%s ",$i); printf("%s\n",$1)}' 
```

# Running in transactional mode
When running ```para``` on very large input files, say ```100M``` lines or larger where each line takes a non-negligible time to process, it can be painful to crash somewhere in the middle and have to start over from scratch.

## introduction

```para```'s transactional feature allows a processing to restart from a well defined point in the processing. For example, when starting ```para``` it is possible to specify that the state of the processing should be saved (i.e. committed) every N (say 1000) lines. At a commit point ```para``` atomically saves two values in a transactional log:

* #of input lines that have been written to the output file

* position in the output file

When ```para``` is restarted in recovery mode it reads the transactional log (default: ```.para.txnlog```). ```para``` then skips the first N lines in the input stream and position itself in the output stream (if the output is a file) before starting to process lines from the input. When it is not possible to position in the output stream ```para``` ignores the output file position from the commit log.

If ```para``` is started in recovery mode when there is no transactional log ```para``` will simply ignore that recovery has been specified. Therefore, it is possible to always run ```para``` in recovery mode.

## why transactions

Would it not be simpler to just scan the output file for the last newline and use that as the 'commit' point? I can see two reasons fro not doing this:

* in general we cannot re-run the ```para``` processing from scratch and reliably reproduce exactly the same result. Say we crash at some point after line 53 has been written to the output file due to processing of input line 61. Next time we run ```para``` we will still crash while processing line 61. However this time we might have written line 56 to the output file. This means that in the first case had we restarted in recovery mode we would start at line 54 whereas in the second case we would start at line 57.
* unless we ```fflush``` after each line written to the output file (which would be very expensive) I cannot see that there is a guarantee that  there might not be holes in the file on disk. That is, it is possible (unless I'm mistaken) that the kernel might not have flushed some buffers that correspond to the output file that are 'in the middle' of the file. I have not seen any documentation that the kernel guarantees that kernel buffers are always flushed sequentially from low positions in the file to high positions in the file. 

On the second point, clearly I might be incorrect ... if so, please correct me.

However, the first point is enough reason for me to use a transactional approach.

## an example:

Say we have ```input.txt```:

```1
1
2
3
4
5
```

and ```exe.bash```:

```#!/bin/bash
while read line; do
  echo $line 
  sleep 1
done
```

and we execute (```-R``` is recovery and ```-C 2``` enables transactions every 2 lines):

```
$>para para -v -i input.txt -o output.out -R -C 2  -- 1 ./exe.bash
```

the ```output.txt``` is then:

```1
1
2
3
4
5
```

Now, if we execute the same command but hit ```^C``` after seeing the message:

```info: committing at 2 lines...```

The output is now:

```
1
2
```

or possibly:

```
1
2
3
```

and the file:

```.para.txnlog```

contains (using ```od -x .para.txnlog```):

```
0000000 0002 0000 0000 0000 0004 0000 0000 0000
0000020
```

We can view the transaction log by executing:

```$>para -r
$para -r
```

```para``` informs that 2 lines were committed and output in recovery mode starts at position 4 in the output file:

```recovery info --> #lines-committed: 2, outfile-pos: 4```

Now we can restart ```para```:

```$>para para -v -i input.txt -o output.out -R -C 2  -- 1 ./exe.bash```

```para``` now writes:

```
info: skipping first: 2 lines in recovery mode, outfilepos: 4 ...
info: positioning to offset: 4 in output stream
info: committing at 4 lines ...
debug: #timers in queue: 1 (expected: 1 HEARTBEAT timer)
info: committing at 5 lines ...
debug: closing files ...
debug: waiting for child processes ...
debug: cleaning up memory ...
debug: ... cleanup done
```

We see that ```para``` skips 2 lines in the input file and positions at file position 4 in the output file before starting to process. The output file is now:

```
1
2
3
4
5
```


# Design and implementation
NOTE! not yet done
* draw diagram showing design
* describe implementation

## Para internals

```para``` is implemented in C. The choice was done mostly for reason of portability. An alternative (and simpler solution) would have been to use C++ together with the ```boost asio``` support libraries. However, portability and ease of installation (avoid having correct installation of C++ support and boost libraries) can quickly become complicated when involving ```C++``` and ```boost```. For a fairly simple program like ```para``` I did not seee the extra implementation effort using ```C``` as a big obstacle.

## the ```select``` call

```para``` is at the core a loop around call to the ```select``` (in the implementation it is actually a call to ```pselect``` so that signals are handled properly) system call. The ```select``` call is configured in each loop with a set of file descriptors and a timeout. 

## buffers

NOTE! not yet done

* buffers
* timers
* communication buffers
* sub-processes
* output queue

# TODOs and Ideas

This section lines out some ideas that have not yet been designed or implemented.

## network input/output

```para``` should be able to read input by connecting to a network based server supplying lines of text. It should then also be able to connect and send output to a network based server. Clearly exactly how to control number of lines read from the network based server must be thought through and specified. Additionally, error handling must be understood and documented.

## sub-processes as network based servers

```para``` currently starts resources (sub-processes) by executing a ```fork```/```exec``` of a program. It should be possible to run ```para``` in an environment where instead ```para``` connects to network based resources and disconnects from them when all input has been processed.

For example, instead of ```fork```/```exec``` of 5 sub-processes ```para``` can create 5 connections to a TCP based server which is already running.

## ```para``` as a network based server

It should be possible to run ```para``` as a server where the server runs N sub-commands continuously. ```para``` running as a client can then connect and submit lines to the server which are then processed in parallel as a sub-commands on the server side.  

## optimization

```para``` currently writes one line at a time to a sub-process. This is not very efficient since ```para``` will perform more context switches than needed.

Instead ```para``` should write multiple lines to a single sub-process in one shot, track the lines that were sent to a sub-process and read multiple lines back from a single sub-process.

## automatic re-sizing of internal data structures

* buffers (```buf```) have fixes size and cannot be re-sized - we can probably live with this but if time allows a better and more flexible buffering machinery should be implemented
* ```combuftab``` (table containing a communication buffer for each sub-process) is not re-sizable - we can probably live with this since we don't create new sub-processes dynamically

## controlling which lines to process

* line number to start processing
* #of lines to process
* process only each Nth line
* process only lines matching regular expression
* process only line numbers matching regular expression

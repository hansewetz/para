# ```para``` <sub><sup>(version 0.2)</sup></sub>

```para``` is a program for efficient parallel processing of linebased text. 
```para``` delegates processing of each line in a file to a user specified sub command which is managed and executed by ```para```. ```para``` collects the output from sub-commands and prints them in the same order as they occurred in the input file.

# A simple example

Say we want to calculate the ```md5sum``` for each line in a file using a script stored in ```md5.bash```. Without thinking too much about efficiency we type up the following script:

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

then generates the output:

```
b1946ac92492d2347c6235b4d2611184
```

Now lets calculate the ```md5sum``` for each line in a medium size file having around 500K lines:

```
$ time zcat | monolingual.en.gz | ./md5.bash > out1.md5sum
```

The execution time is slightly more than 20 minutes:

```
real    21m22.874s
user    10m18.485s
sys     28m48.447s
```

Now we'll use ```para``` to speed up the processing:

```
$ time zcat | monolingual.en.gz | para -- 5 ./md5.bash > out2.md5sum
```

The processing time is now:

```
real    4m34.788s
user    10m42.282s
sys     29m59.132s
```

To make sure ```para``` behaves correctly we'll make sure the output from the ```para``` command generates identical output as the first command executed without ```para```:

```
$ diff out1.md5sum out2.md5sumn | wc -l
0
```

In our example ```para``` starts 5 sub processes running the ```md5.bash``` command. 

We can see that the speedup is approximately 5 times. Linear speedup is not always possible though. When the sub-command does not do much heavy calculations the overhead of ```para``` takes over and the ```para``` solution might perform worse.

# Installing ```para```

Install ```para``` by following the steps:
```
$ mkdir para && cd para
$ git clone https://github.com/hansewetz/para.git .
$ mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX:PATH=<my-install-dir>  .. && make && make install
$ export PATH=${PATH}:<my-install-dir>/bin
```

You can now  execute:

```$> para -- 1 cat
$ para -- 1 cat                          # run with a single sub-process
Hello                                    # your text
Hello                                    # echoed by para
info: timer popped, type: HEARTBEAT      # default heartbeat is every 5 sec
info: timer popped, type: HEARTBEAT      # ...
^D                                       # stop with ^D
```

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
  -b arg      maximum length in bytes of a line (optional, default: 4096)
  -T arg      timeout in seconds waiting for response from a sub-process (default 5)
  -H arg      heartbeat in seconds (optional, default: 5)
  -m arg      #of child processes to spawn (optional if specified as a positional parameter, default: 1)
  -M arg      maximum number of lines to store in the output queue before terminating, if non-zero the queue will be incremented (see '-x' option) (default: 1000)
  -x arg      increment output queue with this #of elements if overflow (default: 1000)
  -c arg      command to execute in child process (optional if specified as positional parameter)
  -i arg      input file (default is standard input, optional)
  -o arg      output file (default is standard output, optional)
  --
  maxclients  #of child processes to spawn (optional if specified as command line parameter, default: 1)
  cmd         command to execute in child processes (optional if specified as '-c' option)
```
```para``` takes a relatively small number of command line parameters. Some parameters control basic functional behavior whereas other are used for tweaking ```para``` at the technical level. Most parameters related to technical issues have reasonable default values and should not have to be specified.

## timeout while waiting for sub-process

After ```para``` sends a line to a sub-process it waits for a response. If the response time is longer than a specified timeout value (default is 5 5 seconds) ```para``` terminates all sub-processes with an error message and exits with a non-zero error code.

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

Sub-commands that buffers input and/or output presents a problem to ```para```. This because ```para``` operates on a line by line basis and processing of a line to occur as soon as the line has been sent to the sub-command. Additionally ```para``` expects the result to be sent back without and delayed buffering. 

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

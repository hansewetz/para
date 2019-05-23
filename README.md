# ```para```

```para```is a program for processing large line based files efficiently.

```para``` delegates processing of each line in the file to a user specified sub command. The output is generated so that the output from the sub command for line N is also line N in the output.

# A simple example

Say we want to process each line in a text file with a command and print the output in the same order as the input. As simple example we run **cat** as the sub-command echoing each line back to ```para``` :

```
para -- 5 cat < input.txt > output.txt
```

```para``` starts 5 sub processes each running the ```cat``` command.

Each line in the file is piped to one of the 5 sub processes. The output from the sub processes are then collected by ```para``` and written to ```stdout``` in the correct order.

The example is somewhat contrived since ```cat``` does not do any time consuming work. The main point in running ```para``` after all to split time consuming work across multiple processes and afterwards assemble the result from sub-processes to reduce end-to-end processing time.

# **para** command line parameters

```para``` is a written and designed as a standard UNIX like utility. It takes named command line parameters, it reads from ```stdin``` and ```stdout```. For backwards compatibility and historical reasons it also supports a few positional command line parameters. Running ```para -h``` produces:

```
Usage:
  para [options] -- maxclients cmd cmdargs ...

options:
  -h          help and exit (default: not set)
  -p          print cmd line parameters (default: not set)
  -v          verbose mode (maximum debug level, default: not set)
  -V          print version number (optional, default: not set)
  -b arg      maximum length in bytes of a line (optional, default: 4096)
  -H arg      heartbeat in seconds (optional, default: 5)
  -m arg      #of child processes to spawn (optional if specified as a positional parameter, default: 1)
  -M arg      maximum number of lines to store in the output queue before terminating (default: 1000)
  -c arg      command to execute in child process (optional if specified as positional parameter)
  -i arg      input file (default is standard input, optional)
  -o arg      output file (default is standard output, optional)
  --
  maxclients  #of child processes to spawn (optional if specified as command line parameter, default: 1)
  cmd         command to execute in child processes (optional if specified as '-c' option)
```
## heartbeat timer

```para``` supports a *heartbeat* timer so that it is possible to see that ```para``` is alive when processing input comes sporadically. For example:

```
# run para with a heartbeat of 2 seconds (-H 2)
while :; do sleep 1; echo hello; done | ./cpp/apps/para/para -H 2 -- 5 cat
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

By default input is read from```stdin``` and output is written to ```stdout```. If one or both of the command line parameters ```-i inputfile``` and ```-o outputfile``` are specified input is read from file and output is written to file instead.

Currently only file input and output is supported in addition to ```stdin``` and ```stdout```. Future extension will support network connections as well.

## internal limits in ```para```

The two (2) important internal limits in ```para``` are:

NOTE! not yet done

# Running sub-commands that buffer

Sub-commands that buffers input and/or output presents a problem to ```para```. This section 

## ```sed``` as a sub-command

 ```sed``` must be run with the flag ```--unbufferd``` when executed as a sub-command to ```para```:

```
echo 'hello again' | para -- 5 sed --unbuffered 's/a/z/g'
```

prints:

```
hello zgzin
```

Without the ```--unbuffered``` flag ```sed``` will simply hang since 

## a general solution

A more general solution to buffering is done using the ``unbuffer`` command:

```
echo 'hello again' | para -- 5 unbuffer -p sed 's/a/z/g'
```

```unbuffer``` runs ```sed``` ensuring that input and output into and from ```sed``` is not buffered.

Running ```awk```as a sub-command presents similar buffering problems that also can be solved using ```unbuffer```. For example, reversing the order of words on each line can be done with the command:

```
para -- 10 unbuffer -p gawk '{for (i=NF; i>1; i--) printf("%s ",$i); printf("%s\n",$1)}' 
```

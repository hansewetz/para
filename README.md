# para

**para** is a program for processing large line based files efficiently.

**para** delegates processing of each line in the file to a user specified sub command. The output is generated so that the output from the sub command for line N is also line N in the output.

# A small example

Say we want to process each line in a file with a command (in this case **cat**) and print the output in the same order as the input. We can do this by executing:

```
para -- 5 cat
```

Here **para** starts five (5) sub processes each running the **cat** command.

Each line in the file is piped to one of the sub processes and the output from the sub process is collected and written  in the correct order to **stdout**

The example is somewhat artificial since **cat** does not do much any really time consuming work.

The main point in using a program like **para** is after all to split time consuming work across multiple processes and afterwards assemble the result in order to reduce end-to-end processing time.

If you have a file ```input.txt```:

```
Hello worl: line 1
Next line: line2
A third line: line 3
```



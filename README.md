# lab0-c
Assessing Your C Programming Skills

## Running the autograders

Before running the autograders, compile your code to create the testing program qtest
```shell
make
```

Check the correctness of your code:
```shell
make test
```

Check the memory issue of your code:

```shell
make valgrind
```

* Modify `./.valgrindrc` to customize arguments of Valgrind
* Use `make clean` or `rm /tmp/qtest.*` to clean the temporary files created by target valgrind

## Using qtest

qtest provides a command interpreter that can create and manipulate queues.

Run `./qtest -h` to see the list of command-line options

When you execute `./qtest`, it will give a command prompt "cmd>".  Type
"help" to see a list of available commands

## Files

You will handing in these two files
* queue.h : Modified version of declarations including new fields you want to introduce
* queue.c : Modified version of queue code to fix deficiencies of original code

Tools for evaluating your queue code
* Makefile : Builds the evaluation program qtest
* README.md : This file
* scripts/driver.py : The C lab driver program, runs qtest on a standard set of traces

Helper files
* console.{c,h} : Implements command-line interpreter for qtest
* report.{c,h} : Implements printing of information at different levels of verbosity
* harness.{c,h} : Customized version of malloc and free to provide rigorous testing framework
* qtest.c : Code for qtest

Trace files
* traces/trace-XX-CAT.cmd : Trace files used by the driver.  These are input files for qtest.
  * They are short and simple.
  * We encourage to study them to see what tests are being performed.
  * XX is the trace number (1-15).  CAT describes the general nature of the test.
* traces/trace-eg.cmd : A simple, documented trace file to demonstrate the operation of qtest

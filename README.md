# lab0-c
Assessing Your C Programming Skills

This lab will give you practice in the style of programming you will need to be able to do proficiently,
especially for the later assignments in the class. The material covered should all be review for you. Some
of the skills tested are:
* Explicit memory management, as required in C.
* Creating and manipulating pointer-based data structures.
* Working with strings.
* Enhancing the performance of key operations by storing redundant information in data structures.
* Implementing robust code that operates correctly with invalid arguments, including NULL pointers.

The lab involves implementing a queue, supporting both last-in, first-out (LIFO) and first-in-first-out (FIFO)
queueing disciplines. The underlying data structure is a singly-linked list, enhanced to make some of the
operations more efficient.

## Prerequisites

There are a few prerequisites which must be installed on your machine before you will
be able to build and run the autograders.

The following command will install all required and optional dependencies on Ubuntu
Linux 18.04 or later:
```shell
$ sudo apt install build-essential git clang-format cppcheck aspell colordiff valgrind
```

Note: [Cppcheck](http://cppcheck.sourceforge.net/) version must be at least 1.90, otherwise
it might report errors with false positives. You can get its version by executing `$ cppcheck --version`.
Check [Developer Info](http://cppcheck.sourceforge.net/devinfo/) for building Cppcheck from source. Alternatively,
you can make use of [snap](https://snapcraft.io/) for latest Cppcheck:
```shell
$ sudo snap install cppcheck
$ export PATH=/snap/bin:$PATH
```

## Integrate clang-format for vim
If you want to run `clang-format` automatically after saving with vim, 
clang-format supports integration for vim according to [Clang 11 documentation](https://clang.llvm.org/docs/ClangFormat.html). 

By adding the following into `~/.vimrc`
```shell
function! Formatonsave()
  let l:formatdiff = 1
  py3f <path to your clang-format.py>/clang-format.py
endfunction
autocmd BufWritePre *.h,*.cc,*.cpp call Formatonsave()
```
Note: on Ubuntu Linux 18.04, the path to `clang-format.py` is `usr/share/vim/addons/syntax/`.  


## Running the autograders

Before running the autograders, compile your code to create the testing program `qtest`
```shell
$ make
```

Check the correctness of your code, i.e. autograders:
```shell
$ make test
```

Check the example usage of `qtest`:
```shell
$ make check
```
Each step about command invocation will be shown accordingly.

Check the memory issue of your code:
```shell
$ make valgrind
```

* Modify `./.valgrindrc` to customize arguments of Valgrind
* Use `$ make clean` or `$ rm /tmp/qtest.*` to clean the temporary files created by target valgrind

Extra options can be recognized by make:
* `VERBOSE`: control the build verbosity. If `VERBOSE=1`, echo eacho command in build process.
* `SANITIZER`: enable sanitizer(s) directed build. At the moment, AddressSanitizer is supported.

## Using qtest

`qtest` provides a command interpreter that can create and manipulate queues.

Run `$ ./qtest -h` to see the list of command-line options

When you execute `$ ./qtest`, it will give a command prompt `cmd> `.  Type
"help" to see a list of available commands.

## Files

You will handing in these two files
* queue.h : Modified version of declarations including new fields you want to introduce
* queue.c : Modified version of queue code to fix deficiencies of original code

Tools for evaluating your queue code
* Makefile : Builds the evaluation program `qtest`
* README.md : This file
* scripts/driver.py : The driver program, runs `qtest` on a standard set of traces

Helper files
* console.{c,h} : Implements command-line interpreter for qtest
* report.{c,h} : Implements printing of information at different levels of verbosity
* harness.{c,h} : Customized version of malloc/free/strdup to provide rigorous testing framework
* qtest.c : Code for `qtest`

Trace files
* traces/trace-XX-CAT.cmd : Trace files used by the driver.  These are input files for `qtest`.
  * They are short and simple.
  * We encourage to study them to see what tests are being performed.
  * XX is the trace number (1-15).  CAT describes the general nature of the test.
* traces/trace-eg.cmd : A simple, documented trace file to demonstrate the operation of `qtest`

## License

`lab0-c`is released under the BSD 2 clause license. Use of this source code is governed by
a BSD-style license that can be found in the LICENSE file.

External source code:
* [dudect](https://github.com/oreparaz/dudect): public domain
* [git-good-commit](https://github.com/tommarshall/git-good-commit): MIT License

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
queueing disciplines. The underlying data structure is a circular doubly-linked list, enhanced to make some of
the operations more efficient.

## Prerequisites

There are a few prerequisites which must be installed on your machine before you will
be able to build and run the autograders.

The following command will install all required and optional dependencies on Ubuntu
Linux 20.04 or later:
```shell
$ sudo apt install build-essential git clang-format cppcheck aspell colordiff valgrind
```
Some distros like Arch Linux won't install `aspell-en` with `aspell`, and you must install it explicitly:
```shell
$ sudo pacman -S aspell-en
```

Note: [Cppcheck](https://cppcheck.sourceforge.io/) version must be at least 1.90, otherwise
it might report errors with false positives. You can get its version by executing `$ cppcheck --version`.
Check [Developer Info](https://cppcheck.sourceforge.io/devinfo/) for building Cppcheck from source.

### Integrate `clang-format` to `vim`
If you want to run `clang-format` automatically after saving with vim, 
clang-format supports integration for vim according to [Clang documentation](https://clang.llvm.org/docs/ClangFormat.html).

By adding the following into `$HOME/.vimrc`
```shell
function! Formatonsave()
  let l:formatdiff = 1
  py3f <path-to-clang-format.py>/clang-format.py
endfunction
autocmd BufWritePre *.h,*.hpp,*.c,*.cc,*.cpp call Formatonsave()
```

Then, it has zero-effort integration into the coding workflow since it can handle formatting changes while saving a file.
Note: on Ubuntu Linux 18.04, the path to `clang-format.py` is `/usr/share/vim/addons/syntax/`.  

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
* `VERBOSE`: control the build verbosity. If `VERBOSE=1`, echo each command in build process.
* `SANITIZER`: enable sanitizer(s) directed build. At the moment, AddressSanitizer is supported.

## Using `qtest`

`qtest` provides a command interpreter that can create and manipulate queues.

Run `$ ./qtest -h` to see the list of command-line options

When you execute `$ ./qtest`, it will give a command prompt `cmd> `.  Type
`help` to see a list of available commands.

## Files

You will handing in these two files
* `queue.h` : Modified version of declarations including new fields you want to introduce
* `queue.c` : Modified version of queue code to fix deficiencies of original code

Tools for evaluating your queue code
* `Makefile` : Builds the evaluation program `qtest`
* `README.md` : This file
* `scripts/driver.py` : The driver program, runs `qtest` on a standard set of traces
* `scripts/debug.py` : The helper program for GDB, executes `qtest` without SIGALRM and/or analyzes generated core dump file.

Helper files
* `console.{c,h}` : Implements command-line interpreter for qtest
* `report.{c,h}` : Implements printing of information at different levels of verbosity
* `harness.{c,h}` : Customized version of malloc/free/strdup to provide rigorous testing framework
* `qtest.c` : Code for `qtest`

Trace files
* `traces/trace-XX-CAT.cmd` : Trace files used by the driver.  These are input files for `qtest`.
  * They are short and simple.
  * We encourage to study them to see what tests are being performed.
  * XX is the trace number (1-17).  CAT describes the general nature of the test.
  * All functions that need to be implemented are explicitly listed.
  * If a colon is present in the title, all functions mentioned afterwards must be correctly implemented for the test to pass.
* `traces/trace-eg.cmd` : A simple, documented trace file to demonstrate the operation of `qtest`

## Debugging Facilities

Before using GDB debug `qtest`, there are some routine instructions need to do. The script `scripts/debug.py` covers these instructions and provides basic debug function. 
```shell
$ scripts/debug.py -h
usage: debug.py [-h] [-d | -a]

optional arguments:
  -h, --help     show this help message and exit
  -d, --debug    Enter gdb shell
  -a, --analyze  Analyze the core dump file
```
* Enter GDB without interruption by **SIGALRM**.
```shell
$ scripts/debug.py -d
Reading symbols from lab0-c/qtest...done.
Signal        Stop	Print	Pass to program	Description
SIGALRM       No	No	No		Alarm clock
Starting program: lab0-c/qtest 
cmd> 
```
* When `qtest` encountered **Segmentation fault** while it ran outside GDB, we could invoke GDB in the post-mortem debugging mode to figure out the bug.

  The core dump file was created in the working directory of the `qtest`.
  * Allow the core dumps by using shell built-in command **ulimit** to set core file size.
  ```shell
  $ ulimit -c unlimited
  $ ulimit -c
  unlimited
  ```
  * Enter GDB and analyze
  ```shell
  $ scripts/debug.py -a
  Reading symbols from lab0-c/qtest...done.
  [New LWP 9424]
  Core was generated by `lab0-c/qtest'.
  Program terminated with signal SIGSEGV, Segmentation fault.
  #0 ...
  #1 ... (backtrace information)
  #2 ...
  (gdb) 
  ```

## User-friendly command line
[linenoise](https://github.com/antirez/linenoise) was integrated into `qtest`, providing the following user-friendly features:
* Move cursor by Left and Right key
* Jump the cursor over words by Ctrl-Left and Ctrl-Right key
* Get previous or next command typed before by up and down key
* Auto completion by TAB

## Built-in web server

A small web server is already integrated within the `qtest` command line interpreter,
and you may use it by running the `web` command in its prompt.
```shell
$ ./qtest
cmd> web
listen on port 9999, fd is 3
```

Run the following commands in another terminal after the built-in web server is ready.
```shell
$ curl http://localhost:9999/new
$ curl http://localhost:9999/ih/1
$ curl http://localhost:9999/ih/2
$ curl http://localhost:9999/ih/3
$ curl http://localhost:9999/sort
$ curl http://localhost:9999/quit
```

## License

`lab0-c` is released under the BSD 2 clause license. Use of this source code is governed by
a BSD-style license that can be found in the LICENSE file.

External source code:
* [dudect](https://github.com/oreparaz/dudect): public domain
* [git-good-commit](https://github.com/tommarshall/git-good-commit): MIT License
* [linenoise](https://github.com/antirez/linenoise): BSD 2-Clause "Simplified" License
* [tiny-web-server](https://github.com/7890/tiny-web-server): MIT License
* [randombytes](https://github.com/dsprenkels/randombytes): MIT License

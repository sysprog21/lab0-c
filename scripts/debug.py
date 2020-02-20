#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import pathlib
import os

class Debugger:
    def __init__(self, command, common_opts):
        self.command = command
        self.common_opts = common_opts

    def __call__(self, argv, **kwargs):
        g = subprocess.Popen([self.command] + self.common_opts + argv, **kwargs)

        while g.returncode != 0:
            try:
                g.communicate()
            except KeyboardInterrupt:
                pass
    
        return g

    def debug(self, program):
        return self([
            "-ex", "handle SIGALRM ignore",
            "-ex", "run",
            program
        ])

    def analyze(self, program, core_file):
        return self([
            program,
            core_file,
            "-ex", "backtrace"
        ])


def main(argv):
    common = [
        "-quiet", 
        f"-cd={ROOT}"
    ]

    gdb = Debugger(GDB_PATH, common)
    
    if argv.debug:
        gdb.debug(QTEST)

    elif argv.analyze:
        if not os.path.exists(CORE_DUMP):
            print("ERROR: core dump file is not exist")
            exit(1)

        gdb.analyze(QTEST, CORE_DUMP)

    else:
        parser.print_help()

if __name__ == "__main__":
    ROOT = str(pathlib.Path(__file__).resolve().parents[1])
    GDB_PATH = shutil.which("gdb")
    QTEST = ROOT + "/qtest"
    CORE_DUMP = ROOT + "/core"

    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-d", "--debug", dest="debug", action="store_true",
                        help="Enter gdb shell")
    group.add_argument("-a", "--analyze", dest="analyze", action="store_true", 
                        help="Analyze the core dump file")
    args = parser.parse_args()

    if not GDB_PATH:
        print("ERROR: gdb is not installed")
        exit(1)

    if not os.path.exists(QTEST):
        print("ERROR: qtest is not exist")
        exit(1)

    main(args)
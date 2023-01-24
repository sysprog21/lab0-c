#!/usr/bin/env bash

SOURCES=$(find $(git rev-parse --show-toplevel) | egrep "\.(cpp|h)\$")

set -x

for file in ${SOURCES};
do
    clang-format-14 ${file} > expected-format
    diff -u -p --label="${file}" --label="expected coding style" ${file} expected-format
done
exit $(clang-format-14 --output-replacements-xml ${SOURCES} | egrep -c "</replacement>")

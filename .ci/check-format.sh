#!/usr/bin/env bash

SOURCES=$(find $(git rev-parse --show-toplevel) | grep -E "\.(cpp|h)\$")

set -x

for file in ${SOURCES};
do
    clang-format-18 ${file} > expected-format
    diff -u -p --label="${file}" --label="expected coding style" ${file} expected-format
done
exit $(clang-format-18 --output-replacements-xml ${SOURCES} | grep -E -c "</replacement>")

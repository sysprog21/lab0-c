#!/usr/bin/env bash

SHA1SUM=$(which sha1sum)
if [ $? -ne 0 ]; then
    SHA1SUM=shasum
fi

$SHA1SUM -c scripts/checksums
if [ $? -ne 0 ]; then
    echo "[!] You are not allowed to change the header file queue.h or list.h" >&2
    exit 1
fi

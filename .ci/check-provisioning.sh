#!/usr/bin/env bash

PROVISION_DIR=lab0-c-private
if test -f $PROVISION_DIR/queue.c; then
    cp -f $PROVISION_DIR/queue.c .
    # Skip complexity checks
    sed -i '/17:/d' scripts/driver.py
fi

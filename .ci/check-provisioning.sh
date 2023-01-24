#!/usr/bin/env bash

PROVISION_DIR=lab0-c-private
if test -f $PROVISION_DIR/queue.c; then
    cp -f $PROVISION_DIR/queue.c .
fi

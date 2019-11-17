#!/bin/sh
set -x
ez80-clang -w -S $CFLAGS -o /dev/null 2>&1 | grep "error in backend: unable to legalize instruction: .*G_ANYEXT"

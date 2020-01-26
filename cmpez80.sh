#!/bin/sh
set -x
! ez80-llc "$@" -o /dev/null 2>&1 | grep "^LLVM ERROR: unable to legalize instruction: "

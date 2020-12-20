#!/bin/bash
done=false
until $done; do
    for opt in 0 1 2 3 s z; do
        if make check TEST_CFLAGS=-O$opt; then
            done=true
            break
        fi
    done
done

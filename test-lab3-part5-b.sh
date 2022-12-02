#!/usr/bin/env bash

ulimit -c unlimited


BASE_PORT=$RANDOM
BASE_PORT=$[BASE_PORT+2000]
EXTENT_PORT=$BASE_PORT
LOCK_PORT=$[BASE_PORT+6]

LogDIR=$PWD/log

# create log dir
if [ ! -d "$LogDIR" ]; then
    mkdir $LogDIR
fi

./test-lab3-part5-b $BASE_PORT 

./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
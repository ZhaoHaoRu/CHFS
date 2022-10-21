#!/usr/bin/env bash

ulimit -c unlimited

LogDIR=$PWD/log
ChFSDIR1=$PWD/chfs1

# create log dir
if [ ! -d "$LogDIR" ]; then
    mkdir $LogDIR
fi

# start chfs1
rm -rf $ChFSDIR1
mkdir $ChFSDIR1 || exit 1
sleep 1
echo "starting ./chfs_client $ChFSDIR1  > chfs_client1.log 2>&1 &"
./chfs_client $ChFSDIR1   > chfs_client1.log 2>&1 &

sleep 2

# make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount ChFS properly at ./chfs1"
    exit -1
fi

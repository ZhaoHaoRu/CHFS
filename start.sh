#!/usr/bin/env bash

ulimit -c unlimited

LOSSY=$1
NUM_LS=$2

if [ -z $NUM_LS ]; then
    NUM_LS=0
fi

BASE_PORT=$RANDOM
BASE_PORT=$[BASE_PORT+2000]
EXTENT_PORT=$BASE_PORT
ChFS1_PORT=$[BASE_PORT+2]
ChFS2_PORT=$[BASE_PORT+4]
LOCK_PORT=$[BASE_PORT+6]

LogDIR=$PWD/log
ChFSDIR1=$PWD/chfs1
ChFSDIR2=$PWD/chfs2

# create log dir
if [ ! -d "$LogDIR" ]; then
    mkdir $LogDIR
fi

# =======start lock server=======
if [ "$LOSSY" ]; then
    export RPC_LOSSY=$LOSSY
fi
if [ $NUM_LS -gt 1 ]; then
    x=0
    rm config
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo $port >> config
    done
    x=0
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo "starting ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &"
      ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &
      sleep 1
    done
else
    echo "starting ./lock_server $LOCK_PORT > lock_server.log 2>&1 &"
    ./lock_server $LOCK_PORT > lock_server.log 2>&1 &
    sleep 1
fi
unset RPC_LOSSY


# =======start extent server=======
echo "starting ./extent_server $EXTENT_PORT > extent_server.log 2>&1 &"
./extent_server $EXTENT_PORT > extent_server.log 2>&1 &
sleep 1


# =======start chfs client=======
rm -rf $ChFSDIR1
mkdir $ChFSDIR1 || exit 1
sleep 1
echo "starting ./chfs_client $ChFSDIR1 $EXTENT_PORT $LOCK_PORT > chfs_client1.log 2>&1 &"
./chfs_client $ChFSDIR1 $EXTENT_PORT $LOCK_PORT > chfs_client1.log 2>&1 &
sleep 1

rm -rf $ChFSDIR2
mkdir $ChFSDIR2 || exit 1
sleep 1
echo "starting ./chfs_client $ChFSDIR2 $EXTENT_PORT $LOCK_PORT > chfs_client2.log 2>&1 &"
./chfs_client $ChFSDIR2 $EXTENT_PORT $LOCK_PORT > chfs_client2.log 2>&1 &
sleep 2


# =======check FUSE mounted=======
# make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount ChFS properly at ./chfs1"
    exit -1
fi
# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/chfs2" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount ChFS properly at ./chfs2"
    exit -1
fi

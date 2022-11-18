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

ChFSDIR1=$PWD/chfs1


# =======start extent server=======
# echo "starting ./extent_server $EXTENT_PORT > extent_server.log 2>&1 &"
# ./extent_server $EXTENT_PORT > extent_server.log 2>&1 &
# sleep 1

# =======start extent server raft group=======
echo "starting ./extent_server_dist $EXTENT_PORT > extent_server_dist.log 2>&1 &"
./extent_server_dist $EXTENT_PORT > extent_server_dist.log 2>&1 &
sleep 1

# =======start chfs client=======
rm -rf $ChFSDIR1
mkdir $ChFSDIR1 || exit 1
sleep 1
echo "starting ./chfs_client $ChFSDIR1 $EXTENT_PORT > chfs_client1.log 2>&1 &"
./chfs_client $ChFSDIR1 $EXTENT_PORT > chfs_client1.log 2>&1 &
sleep 1


# =======check FUSE mounted=======
# make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount ChFS properly at ./chfs1"
    exit -1
fi


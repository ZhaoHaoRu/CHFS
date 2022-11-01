#!/usr/bin/env bash

ChFSDIR1=$PWD/chfs1
ChFSDIR2=$PWD/chfs2

# unmount
export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -uz";
fi
$UMOUNT $ChFSDIR1
$UMOUNT $ChFSDIR2

# stop servers
killall extent_server
killall chfs_client
killall lock_server

# rm log dir(no persistence requirement in Lab 2B)
rm log -r

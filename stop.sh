#!/usr/bin/env bash

ChFSDIR1=$PWD/chfs1

# unmount
export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -uz";
fi
$UMOUNT $ChFSDIR1

# stop servers
# killall extent_server
killall extent_server_dist
killall chfs_client


#!/bin/bash

##########################################
#  this file contains:
#   SYMLINK TEST: test for symlink correctness
###########################################

DIR=$1
ORIG_FILE=/etc/hosts

echo "SYMLINK TEST"
rm ${DIR}/hosts ${DIR}/testhostslink ${DIR}/hosts_copy ${DIR}/hosts_copy2 >/dev/null 2>&1

ln -s ${ORIG_FILE} ${DIR}/hosts
diff ${ORIG_FILE} ${DIR}/hosts >/dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "failed SYMLINK test"
    exit
fi

cp ${ORIG_FILE} ${DIR}/hosts_copy
ln -s hosts_copy ${DIR}/testhostslink
diff ${DIR}/testhostslink ${DIR}/hosts_copy >/dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "failed SYMLINK test"
    exit
fi

rm ${DIR}/hosts_copy 
touch ${DIR}/hosts_copy
diff ${DIR}/testhostslink ${DIR}/hosts_copy >/dev/null 2>&1
if [ $? -ne 0 ];
then 
    echo "failed SYMLINK test"
    exit
fi


# crash chfs && wait for it to unmount
echo "===== ChFS Crash =====\n"
# pkill -SIGUSR1 chfs_client
./stop.sh
# if [ $? -ne 0 ];
# then 
#     echo "Failed to crash ChFS: $!\n"
#     exit
# fi
while [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -eq 1 ]
do
    echo "Wait for ChFS to unmount...\n"
    sleep 0.1
done

# restart chfs && wait for it to mount
echo "===== ChFS Restart =====\n";
./start.sh
while [ `mount | grep "$pwd/chfs1" | grep -v grep | wc -l` -ne 1 ]
do
    echo "Wait for ChFS to mount...\n"
    sleep 0.1
done


# recheck all symlink above
diff ${ORIG_FILE} ${DIR}/hosts >/dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "failed SYMLINK test"
    exit
fi

diff ${DIR}/testhostslink ${DIR}/hosts_copy >/dev/null 2>&1
if [ $? -ne 0 ];
then 
    echo "failed SYMLINK test"
    exit
fi

echo "Passed SYMLINK TEST"

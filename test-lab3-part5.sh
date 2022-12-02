#!/usr/bin/env bash

./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
score=0

./start.sh >/dev/null 2>&1
test_if_has_mount(){
	mount | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your ChFS client has failed to mount its filesystem!"
			exit
	fi;
	chfs_count=$(ps -e | grep -o "chfs_client" | wc -l)
	extent_count=$(ps -e | grep -o "extent_server_d" | wc -l)

	if [ $chfs_count -ne 1 ];
	then
			echo "error: chfs_client not found"
			exit
	fi;

	if [ $extent_count -ne 1 ];
	then
			echo "error: extent_server_dist not found"
			exit
	fi;
}
test_if_has_mount

###########################################################
#test basic extent server dist
perl ./test-lab3-part5-a.pl chfs1 | grep -q "Passed all tests"
if [ $? -ne 0 ];
then
        echo "Failed test-lab3-part5: basic extent server dist"
        #exit
else

	ps -e | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: chfs_client DIED!"
			exit
	else
		score=$((score+5))
		#echo $score
		echo "Passed basic chfs raft"
	fi

fi

###########################################################
#test extent server dist for raft
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./test-lab3-part5-b.sh > test-lab3-part5-b.log
if ( ! ( grep -q "pass chfs persist" test-lab3-part5-b.log));
    then
        echo "Failed test chfs persist"
        # exit
else
    score=$((score+5))
    echo "Passed test chfs persist"
fi


echo "Final score of Part5:" $score "/10"
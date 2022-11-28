#!/bin/bash
make clean &> /dev/null 2>&1
echo "Building lab3 tests"
make &> /dev/null 2>&1 

./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1

score=0

mkdir chfs1 >/dev/null 2>&1

###########################################################
#run test raft library
raft_test_case() {
	i=1
    # echo "Testing " $1"."$2;
	while(( $i<=3 ))
	do
		./raft_test $1 $2 | grep -q "Pass ("$1"."$2")";
		if [ $? -ne 0 ];
		then
            echo "Fail " $1"."$2;
			return
		fi;
		let "i++"
	done
    echo "Passed " $1"."$2;
	score=$((score+$3))
}

raft_test_case part1 leader_election 10
raft_test_case part1 re_election 10
raft_test_case part2 basic_agree 10
raft_test_case part2 fail_agree 10
raft_test_case part2 fail_no_agree 5
raft_test_case part2 concurrent_start 5
raft_test_case part2 rejoin 5
raft_test_case part2 backup 5
raft_test_case part2 rpc_count 5
raft_test_case part3 persist1 5
raft_test_case part3 persist2 5
raft_test_case part3 persist3 5
raft_test_case part3 figure8 2
raft_test_case part3 unreliable_agree 2
raft_test_case part3 unreliable_figure_8 1
raft_test_case part4 basic_snapshot 2
raft_test_case part4 restore_snapshot 2
raft_test_case part4 override_snapshot 1

###########################################################
./start.sh
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



echo "Final score:" $score "/100"



make clean &> /dev/null 2>&1
echo "Building lab3 tests"
make &> /dev/null 2>&1 

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

raft_test_case part2 fail_no_agree 5
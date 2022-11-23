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

raft_test_case part1 re_election 10
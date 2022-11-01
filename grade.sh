#!/bin/bash
make clean &> /dev/null 2>&1
make &> /dev/null 2>&1 

./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1

score=0

mkdir chfs1 >/dev/null 2>&1
mkdir chfs2 >/dev/null 2>&1

./start.sh

test_if_has_mount(){
	mount | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your ChFS client has failed to mount its filesystem!"
			exit
	fi;
	chfs_count=$(ps -e | grep -o "chfs_client" | wc -l)
	extent_count=$(ps -e | grep -o "extent_server" | wc -l)

	if [ $chfs_count -ne 2 ];
	then
			echo "error: chfs_client not found (expecting 2)"
			exit
	fi;

	if [ $extent_count -ne 1 ];
	then
			echo "error: extent_server not found"
			exit
	fi;
}
test_if_has_mount

##################################################

# run test 1
perl ./test-lab2b-part1-a.pl chfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-A"
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
		echo "Passed part1 A"
	fi

fi
test_if_has_mount

##################################################

perl ./test-lab2b-part1-b.pl chfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-B"
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
		echo "Passed part1 B"
	fi

fi
test_if_has_mount

##################################################

perl ./test-lab2b-part1-c.pl chfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-c"
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
		echo "Passed part1 C"
	fi

fi
test_if_has_mount

##################################################


./test-lab2b-part1-d.sh chfs1 >tmp.1
./test-lab2b-part1-d.sh chfs2 >tmp.2
lcnt=$(cat tmp.1 tmp.2 | grep -o "Passed SYMLINK" | wc -l)

if [ $lcnt -ne 2 ];
then
        echo "Failed test-part1-d"
        #exit
else

	ps -e | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: chfs_client DIED!"
			exit
	else
		score=$((score+5))
		echo "Passed part1 D"
		#echo $score
	fi

fi
test_if_has_mount

rm tmp.1 tmp.2

##################################################################################

./test-lab2b-part1-e.sh chfs1 >tmp.1
./test-lab2b-part1-e.sh chfs2 >tmp.2
lcnt=$(cat tmp.1 tmp.2 | grep -o "Passed BLOB" | wc -l)

if [ $lcnt -ne 2 ];
then
        echo "Failed test-part1-e"
else
        #exit
		ps -e | grep -q "chfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: chfs_client DIED!"
				exit
		else
			score=$((score+5))
			echo "Passed part1 E"
			#echo $score
		fi
fi

test_if_has_mount

rm tmp.1 tmp.2
##################################################################################
robust(){
./test-lab2b-part1-f.sh chfs1 | grep -q "Passed ROBUSTNESS test"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-f"
else
        #exit
		ps -e | grep -q "chfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: chfs_client DIED!"
				exit
		else
			score=$((score+5))
			echo "Passed part1 F -- Robustness"
			#echo $score
		fi
fi

test_if_has_mount
}



##################################################################################
consis_test(){
./test-lab2b-part1-g chfs1 chfs2 | grep -q "test-lab2b-part1-g: Passed all tests."
if [ $? -ne 0 ];
then
        echo "Failed test-part1-g"
else
        #exit
		ps -e | grep -q "chfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: chfs_client DIED!"
				exit
		else
			score=$((score+5))
			echo "Passed part1 G (consistency)"
			#echo $score
		fi
fi
}

consis_test

if [ $score -eq 30 ];
then
	echo "lab2b part 1 passed"
else
	echo "lab2b part 1 failed"
fi

test_if_has_mount

##################################################################################

./stop.sh >/dev/null 2>&1
sleep 1
mkdir chfs1 >/dev/null 2>&1
mkdir chfs2 >/dev/null 2>&1
./lock_server 3772 > lock_server.log 2>&1 &

./lock_tester 3772 | grep -q "./lock_tester: passed all tests successfully"
if [ $? -ne 0 ];
then
	echo "Failed test-part2"
	#exit
else
	score=$((score+20))
	#echo $score
	echo "Passed part2"
fi

killall lock_server
./start.sh

##################################################################################
./test-lab2b-part3-a chfs1 chfs2 | tee tmp.0
lcnt=$(cat tmp.0 | grep -o "OK" | wc -l)

if [ $lcnt -ne 5 ];
then
        echo "Failed test-part3-a: pass "$lcnt"/5"
	score=$((score+$lcnt*10))
else
        #exit
		ps -e | grep -q "chfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: chfs_client DIED!"
				exit
		else
			score=$((score+25))
			echo "Passed part3 A"
			#echo $score
		fi
fi

rm tmp.0

test_if_has_mount

##################################################################################
./test-lab2b-part3-b chfs1 chfs2 | tee tmp.0
lcnt=$(cat tmp.0 | grep -o "OK" | wc -l)

if [ $lcnt -ne 1 ];
then
        echo "Failed test-part3-b"
else
        #exit
		ps -e | grep -q "chfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: chfs_client DIED!"
				exit
		else
			score=$((score+25))
			echo "Passed part3 B"
			#echo $score
		fi
fi

rm tmp.0

# finally reaches here!
#echo "Passed all tests!"

./stop.sh
echo ""
echo "Score: "$score"/100"
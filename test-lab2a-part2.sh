#!/bin/bash
# make clean &> /dev/null 2>&1
# make &> /dev/null 2>&1 
# mkdir chfs1 >/dev/null 2>&1

#!/bin/bash
score=0

rm log -r
./stop.sh
./start.sh

test_if_has_mount(){
	mount | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your ChFS client has failed to mount its filesystem!"
			echo ""
			echo "Part2 score: "$score"/40"
			exit
	fi;
}
test_if_has_mount

##################################################

# run test a
# perl ./test-lab2a-part2-a.pl chfs1 | grep -q "Passed all"
# if [ $? -ne 0 ];
# then
#         echo "Failed test-A"
#         #exit
# else

# 	ps -e | grep -q "chfs_client"
# 	if [ $? -ne 0 ];
# 	then
# 			echo "FATAL: chfs_client DIED!"
# 			exit
# 	else
# 		score=$((score+20))
# 		#echo $score
# 		echo "Passed A"
# 	fi

# fi
# test_if_has_mount

##################################################

# run test b
perl ./test-lab2a-part2-b.pl chfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-B"
        #exit
else
	
	ps -e | grep -q "chfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: chfs_client DIED!"
			exit
	else
		score=$((score+20))
		#echo $score
		echo "Passed B"
	fi

fi
test_if_has_mount

# finally reaches here!
echo ""
echo "Part2 score: "$score"/20"
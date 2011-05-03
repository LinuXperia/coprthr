#!/bin/sh

run_test() {

	./getndev.x --dev >& /dev/null
	ndev=$?
	for (( i=0; i<$ndev; i++ ))
	do 
		printf "%-68s" "$1 $2 $3 (stddev $i)"
		$1 --size $2 --blocksize $3 --dev $i >& /dev/null
		if [ $? -eq 0 ]; then
			printf "%10s\n" "[pass]"
		else 
			printf "%10s\n" "[failed]"
		fi;
	done

	./getndev.x --cpu >& /dev/null
	ndev=$?
	for (( i=0; i<$ndev; i++ ))
	do 
		printf "%-68s" "$1 $2 $3 (stdcpu $i)"
		$1 --size $2 --blocksize $3 --cpu $i >& /dev/null
		if [ $? -eq 0 ]; then
			printf "%10s\n" "[pass]"
		else 
			printf "%10s\n" "[failed]"
		fi;
	done

	./getndev.x --gpu >& /dev/null
	ndev=$?
	for (( i=0; i<$ndev; i++ ))
	do
		printf "%-68s" "$1 $2 $3 (stdgpu $i)"
		$1 --size $2 --blocksize $3 --gpu $i >& /dev/null
		if [ $? -eq 0 ]; then
			printf "%10s\n" "[pass]"
		else 
			printf "%10s\n" "[failed]"
		fi;
	done

}

echo -e "\nRUNNING TESTS ...";

for t in $*; do

run_test ./$t  65536  16

done

echo -e "... TESTS COMPLETE.";


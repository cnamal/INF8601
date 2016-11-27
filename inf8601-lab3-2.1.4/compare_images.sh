#!/bin/bash

do_run() {
	cmd="mpirun -np $1 ./src/heatsim --input images/earth-medium.png --dimx $2 --dimy $3 --iter 2000 --output simple_$1_$2_$3.png"
	$cmd
	RET=$?
	if [ $RET -ne 0 ]; then
		echo "error $cmd"
		exit 1
	else
		echo "success $cmd"
	fi
    echo "simple_$1_$2_$3.png" > tmp
    ./compare_images.py < tmp
    notify-send "simple_$1_$2_$3.png comparaison finished" 
    rm tmp
}

#do_run 1 1 1
#do_run 2 2 1
#do_run 2 1 2
#do_run 3 3 1
#do_run 3 1 3
#do_run 4 4 1
#do_run 4 1 4
#do_run 4 2 2
#do_run 5 1 5
#do_run 5 5 1
#do_run 6 1 6
#do_run 6 6 1
#do_run 6 2 3
do_run 6 3 2

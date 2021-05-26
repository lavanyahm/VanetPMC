#!/bin/bash
#This file is to run cod with various combinations
if [ "$1" != "" ]; then

        numberOfRuns=$1

	i=0
	sleep 1
	# -lt is less than operator 
  	#Iterate the loop until a less than 10 
	while [ $i -lt $numberOfRuns ] 
	do       
		./waf --run "scratch/project    --n_nodes=16  --speed_min=0  --speed_max=10  " 
		sleep 1   		
		# increment the value 
    		i=`expr $i + 1` 
		echo $i
	done 

	i=0
	sleep 1
	# -lt is less than operator 
  	#Iterate the loop until a less than 10 
	while [ $i -lt $numberOfRuns ] 
	do       
		./waf --run "scratch/project    --n_nodes=64  --speed_min=10  --speed_max=30  " 
		sleep 1   		
		# increment the value 
    		i=`expr $i + 1` 
		echo $i
	done 

	i=0
	sleep 1
	# -lt is less than operator 
  	#Iterate the loop until a less than 10 
	while [ $i -lt $numberOfRuns ] 
	do       
		./waf --run "scratch/project    --n_nodes=64  --speed_min=30  --speed_max=40  " 
		sleep 1   		
		# increment the value 
    		i=`expr $i + 1` 
		echo $i
	done 

	i=0
	sleep 1
	# -lt is less than operator 
  	#Iterate the loop until a less than 10 
	while [ $i -lt $numberOfRuns ] 
	do       
		./waf --run "scratch/project    --n_nodes=64  --speed_min=40  --speed_max=50  " 
		sleep 1   		
		# increment the value 
    		i=`expr $i + 1` 
		echo $i
	done 



else
	echo "Provide arguments :[ Numner of Runs]"
fi	


#!/bin/bash
if [ "$1" != "" ]; then
	./waf --run scratch/project >> outPut.log
	sleep 1
	
	./waf --run scratch/project >> outPut.log
	sleep 1


	./waf --run scratch/project >> outPut.log
	sleep 1
else
	echo "Provide runNumbers  "
fi	






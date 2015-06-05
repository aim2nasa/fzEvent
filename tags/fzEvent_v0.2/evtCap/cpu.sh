#!/bin/sh

while true 
do
	adb -s $1 shell top | grep dmt
done

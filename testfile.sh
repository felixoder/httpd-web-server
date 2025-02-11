#!/bin/bash


let "i=0"

var="abcdefghijklmnopqrstuvwxyz"
len=${#var}

if [ "$1" = ""]; then
	 >&2 echo "Usage: ./testfile.sh <number of bytes>"
	 exit 1

fi

let "chars=$1"



while let "chars"; do

	let "i++"
	#echo -n "$i"


	for x in $(seq 0 $(($len - 1))); do
		char="${var:$x:1}"
		echo "$char"
		let -n "chars--"
		if let "chars < 1"; then
			break
		fi
	done

done






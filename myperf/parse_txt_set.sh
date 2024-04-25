#!/bin/bash

if [ ! "$1" ]; then
	echo "Usage: parse_txt_set <txt dataset file>"
	exit 1
fi

if [ ! -f "$1" ]; then
	echo "$1: No such file or directory"
	exit 1
fi

cat "$1" | grep -Po ' 1\s+\d+\.\d+ [KMG]bits/sec' | grep -Po '\d+\.\d+.+' \
	| awk '{if(NR % 21 != 0) {print $0} else {print ""}}'

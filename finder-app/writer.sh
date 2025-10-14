#!/bin/bash
if [ $# -lt 2 ] 
then
	echo "Usage: $0 <path/to/file> <string to write>"
	exit 1
fi
directory=$(dirname "$1")
mkdir -p "$directory"
echo "$2" > "$1"
exit 0

#!/bin/sh
if [ $# -lt 2 ]
then
	echo "Usage: $0 <path/to/directory> <string to search>"
	exit 1
fi
if [ -e $1 ]
then 
	number_of_files=0
	number_of_lines=0
	for file in "$1"/*; do
		if [ -f "$file" ]
		then
			number_of_files=$((number_of_files+1))
			lines=$(grep -c "$2" "$file")
			number_of_lines=$((number_of_lines+lines))
		fi

	done	
else
	echo -e "$1 is not a directory\nUsage: $0 <path/to/directory> <string to search>"
	exit 1;
fi

echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"

exit 0

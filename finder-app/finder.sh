#!/bin/sh
# Script that helps look for matching lines within files of a directory on the Linux filesystem
# Author: Juncheng Man

# Take in user arguments for the path of directory to search under and the text string to match in files

# ERROR CASES

# If no parameters specified
if [ "$#" -eq 0 ]
then
    	echo "Incomplete arguments: directory path AND search string not provided!"
    	exit 1
fi
    	
# If missing one argument
if [ "$#" -eq 1 ]
then
	echo "Incomplete arguments: EITHER directory path OR search string not provided!"
	exit 1
fi

# If excessive arguments
if [ "$#" -ne 2 ]
then
	echo "Illegal arguments: Only provide directory path and search string!"
	exit 1
fi

# CORRECT ARGUMENTS
	
# If specified directory does not exist, we return value 1 error
# Else, we pass argument value to FILESDIR variable

FILESDIR=$1
if [ ! -d "$FILESDIR" ]
then
	echo "Path:$FILESDIR does not exist."
	exit 1
fi

# Pass argument value to SEARCHSTR variable
SEARCHSTR=$2


# Iterate through all the files in specified directory and its subdirectory recursively (find)
# Look for files of type file (-type f)
# Count number of newline filenames grep outputs to give FILENUMBER (wc -l)
FILENUMBER=$(find "$FILESDIR" -type f | wc -l)

# Iterate through all the files in specified directory and its subdirectory recursively (-R)
# Look for matching string using fixed strings (-F)
# Count number of newline grep outputs to give MATCHINGLINES (wc -l)
MATCHINGLINES=$(grep -RF "$SEARCHSTR" "$FILESDIR" |  wc -l)

echo "The number of files are ${FILENUMBER} and the number of matching lines are ${MATCHINGLINES}"



#!/bin/sh
# Script that creates or overwrites a file with user-defined content
# Author: Juncheng Man

# Take in user arguments for the path of file to write to and the text string to write in file

# ERROR CASES

# If no parameters specified
if [ "$#" -eq 0 ]
then
    	echo "Incomplete arguments: file path AND write string not provided!"
    	exit 1
fi
    	
# If missing one argument
if [ "$#" -eq 1 ]
then
	echo "Incomplete arguments: EITHER file path OR write string not provided!"
	exit 1
fi

# If excessive arguments
if [ "$#" -ne 2 ]
then
	echo "Illegal arguments: Only provide file path and write string!"
	exit 1
fi

# CORRECT ARGUMENTS
	
# Pass argument value to WRITEFILE variable
WRITEFILE=$1

# Pass argument value to WRITESTR variable
WRITESTR=$2

# If file and parent directory do not exist, create parent directory first 
DIRNAME="$(dirname $WRITEFILE)"
if [ ! -f "$WRITEFILE" ] && [ ! -d "$DIRNAME" ]
then
	mkdir -p $DIRNAME
fi

# Create file or overwrite existing content
# Catch if file cannot be created and exit (write command ends with non-zero status)
(echo "$WRITESTR" > "$WRITEFILE") || (echo "ERROR: write file could not be created."; exit 1)






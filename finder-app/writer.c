#include<stdio.h>
#include<stdlib.h>
#include<syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	/*
	* Connects to system logger with no options and LOG_USER facility for user-level logging
	* NULL ident string means program name prepended to every logged message
	*/
	openlog (NULL, 0, LOG_USER);
	
	/*
	* Invalid arguments check
	* Invalid argument error logged with LOG_ERR level, program terminates with status 1
	*/
	if (argc!=3)
	{
		printf("\nIllegal arguments! %d arguments detected\n", argc);
		printf("Usage: %s [file_path] [file_content]\n", argv[0]);
		printf("Program writes [file_content] to file located at [file_path]\n\n");
		syslog(LOG_ERR, "Illegal arguments: %d arguments detected", argc);
		return 1;
	}
	
	/*
	* Assumption: File directory in [filePath] created by caller
	* Opens the file with [filePath] with read, write options (O_RDWR)
	* Else, creates new file (O_CREAT) with RWX permissions for all
	*/
	
	const char *file_path = argv[1];
	int fd = open(file_path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	
	/* Error opening file: error printed and logged and program terminates with status 1 */
	if (fd==-1) 
	{
		printf("Error: (%s) while opening %s\n\n", strerror(errno), file_path);
		syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), file_path);
		return 1;
	}
	
	/*
	*  Writes entirety of fileContent to opened file from starting address of content string
	*/
	const char *file_content = argv [2];
	int bytes_written = write(fd, file_content, strlen(file_content));
	syslog (LOG_DEBUG, "Writing %s to %s", file_content, file_path);
	
	/* Error writing to file: error printed and logged and program terminates with status 1 */
	if (bytes_written==-1)
	{
		printf("Error: (%s) while writing to %s\n\n", strerror(errno), file_path);
		syslog(LOG_ERR, "Error: (%s) while writing to %s", strerror(errno), file_path);
		return 1;
	} 
	/* Incomplete write to file: error printed and logged */
	else if (bytes_written < strlen(file_content))
	{
		printf("Error: Incomplete write to %s, expected: %ld bytes but wrote: %d bytes\n\n", file_path, strlen(file_content), bytes_written);
		syslog(LOG_ERR, "Error: Incomplete write to %s, expected: %ld bytes but wrote: %d bytes", file_path, strlen(file_content), bytes_written);
	}
	
	/* Terminate logger connection */
	closelog ();

}

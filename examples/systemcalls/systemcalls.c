#include "systemcalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

	/*
	*  Call the system() function with the command set in the cmd
	*  and return a boolean true if the system() call completed with success
	*  or false() if it returned a failure
	*/  
	int status = system(cmd);
	
	/* 
	* Successful system() call returns the termination status of cmd executed in child shell, which should be 0 if successful
	* 0 can also be returned when command is NULL, so a check is needed for that
	*/
	if (status==0 && cmd!=NULL) {
		printf("ERROR: Unsuccessful system call\n\n");
		return true;
	}
	return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
	command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	
	/* Avoid duplicate printf */
	fflush(stdout);
	
  	/* Create a new process by forking to execute referenced command */
  	pid_t command_pid = fork();
  	
	/* Check for forking error */
	if(command_pid == -1) {
		printf("ERROR: Unsuccessful process forking\n\n");
		return false;
	}
	
  	/* Now in child process, check for successsful fork with status 0 */
	else if (command_pid == 0) {
		char *command_path = command[0];
		
		/* Execute command from path with arguments starting from second element of command */
		execv(command_path, command);
	
		/* If program got here, execv returned meaning error occurred */
		printf("ERROR: execv returned without successful command execution\n\n");
		exit(1);
		return false;
	}
	else {
		int command_status;
		int wait_status = waitpid(command_pid, &command_status, 0);
		
		/* Check for waiting error */
		if (wait_status == -1) {
			printf("ERROR: unsuccessful wait for child process\n\n");
			return false;
		}
		 if (WIFEXITED(command_status)) {
	 		int command_code = WEXITSTATUS(command_status);
			printf("Process exited, status=%d\n", command_code);
			/* Check for executed command error */
			if (command_code!=0) {
				printf("ERROR: unsuccessful execution of command in child process\n\n");
				return false;
			}
		} else if (WIFSIGNALED(command_status)) {
			printf("Process killed by signal %d\n", WTERMSIG(command_status));
		} else if (WIFSTOPPED(command_status)) {
			printf("Process stopped by signal %d\n", WSTOPSIG(command_status));
		} else if (WIFCONTINUED(command_status)) {
			printf("Process continued\n");
		}
	}
	
	va_end(args);
	printf("\n");
	return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
	command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	// this line is to avoid a compile warning before your implementation is complete
	// and may be removed
	command[count] = command[count];
		
	/* Avoid duplicate printf */
	fflush(stdout);
	
  	/* Create a new process by forking to execute referenced command */
  	pid_t command_pid = fork();
	int fd = open(outputfile, O_RDWR | O_TRUNC | O_CREAT , S_IRWXU | S_IRWXG | S_IRWXO);
	/* Check for file creation/opening error */
	if (fd==-1) 
	{
		printf("Error: (%s) while opening %s\n\n", strerror(errno), outputfile);
		return false;
	}
	/* Check for forking error */
	if(command_pid == -1) {
		printf("ERROR: Unsuccessful process forking\n\n");
		return false;
	}
	
  	/* Now in child process, check for successsful fork with status 0 */
	else if (command_pid == 0) {
		/* With reference from: https://stackoverflow.com/a/13784315/1446624*, redirect standard out (fd 1) to a file specified by outputfile */
		/* Check for file descriptor redirecting error */
		int redirect_status = dup2(fd, 1);
		if (redirect_status < 0) 
		{ 	
			printf("Error: Redirecting outputfile: %s failed\n\n", outputfile);
			return false;
		}
    		close(fd);
		char *command_path = command[0];
		
		/* Execute command from path with arguments starting from second element of command */
		execv(command_path, command);
	
		/* If program got here, execv returned meaning error occurred */
		printf("ERROR: execv returned without successful command execution\n\n");
		exit(1);
		return false;
	}
	else {	
		int command_status;
		int wait_status = waitpid(command_pid, &command_status, 0);
		
		/* Check for waiting error */
		if (wait_status == -1) {
			printf("ERROR: unsuccessful wait for child process\n\n");
			return false;
		}
		close(fd);
		
		if (WIFEXITED(command_status)) {
	 		int command_code = WEXITSTATUS(command_status);
			printf("Process exited, status=%d\n", command_code);
			/* Check for executed command error */
			if (command_code!=0) {
				printf("ERROR: unsuccessful execution of command in child process\n\n");
				return false;
			}
		} else if (WIFSIGNALED(command_status)) {
			printf("Process killed by signal %d\n", WTERMSIG(command_status));
		} else if (WIFSTOPPED(command_status)) {
			printf("Process stopped by signal %d\n", WSTOPSIG(command_status));
		} else if (WIFCONTINUED(command_status)) {
			printf("Process continued\n");
		}
	}
	va_end(args);
	printf("\n");
	return true;
}

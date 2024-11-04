#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <syslog.h>
#include <signal.h>
#include "queue.h"
#include <time.h>
#include "aesdsocket.h"
#include <sys/ioctl.h>
#include "aesd_ioctl.h"

#define TCP_PORT "9000" /* Port for socket cnnection */
#define BACKLOG 5     /* Number of pending connections in queue */
#define USE_AESD_CHAR_DEVICE 1
#ifdef USE_AESD_CHAR_DEVICE
	#define FILE_PATH "/dev/aesdchar" /* File to write data to and send data from */
#else
	#define FILE_PATH "/var/tmp/aesdsocketdata" /* File to write data to and send data from */
#endif
#define BUFFERSIZE 500 /* Buffer for storing received data from client */
#define TIMESTAMP_DUR 10 /* Period of writing timestamp (seconds) */

static volatile bool signal_caught = false;
int totalBytes = 0;

/* Signal handler for SIGINT and SIGTERM,  save a copy of errno to restore later. */
static void signalHandler(int signal_number){
	if (signal_number == SIGINT || signal_number == SIGTERM) {
		int errno_saved = errno;
		signal_caught = true;
		syslog(LOG_DEBUG, "Caught signal, exiting");
	    	errno = errno_saved;
    	}
}

/* Thread function to receive data from client, write to file and send back file content to client */
void* threadFunc(void* thread_func_args)
{		
	/* Cast function arguments as thread params */
	struct connThread* thread_param = (struct connThread*) thread_func_args;
	
	/* Get the ip address of the connecting client */
	char ipv4Addr[INET_ADDRSTRLEN];
	struct sockaddr *incomingSockAddr = (struct sockaddr *)&thread_param->incomingAddr;
    	void *binAddr = &(((struct sockaddr_in  *)incomingSockAddr)->sin_addr);
    	const char *res = inet_ntop(thread_param->incomingAddr.ss_family, &binAddr, ipv4Addr, sizeof(ipv4Addr));
    	if (res == NULL){
		syslog(LOG_ERR, "Error converting client IP Address with error: %s", strerror(errno));
		return thread_func_args;
    	}
	syslog(LOG_DEBUG, "Accepted connection from %s", ipv4Addr);
	
	/* Receive latest data packet from client into buffer */
	int bytesRead  = 0;
	char inBuf[BUFFERSIZE];
	bool endStream = false;
	int rc = 0;
	
	/* While data comes in through the stream */
	while(!endStream){
		bytesRead = recv(thread_param->connFd, inBuf, sizeof(inBuf), 0);
		if(bytesRead <= 0){
			endStream = true;
			break;
		}
		
		int fd = open(FILE_PATH, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
		/* Error opening file: error printed and logged and program terminates with status 1 */
		if (fd ==-1) {
			syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), FILE_PATH);
			pthread_mutex_unlock(thread_param->writeMutex);
			return thread_func_args;
		}
	    	/* If string sent over the socket equals AESDCHAR_IOCSEEKTO:X,Y, do not write */
		if (strstr(inBuf, "AESDCHAR_IOCSEEKTO:") != NULL){
		
			const char *loc = strstr(inBuf, "AESDCHAR_IOCSEEKTO:");
		
			/* Construct seekto struct */
			struct aesd_seekto seek_details;
			
			/* Extract X and Y */
			char *cmd = strtok((char *)loc + 19, ",");
			seek_details.write_cmd = atoi(cmd);
			cmd = strtok(NULL, ",");
			seek_details.write_cmd_offset = atoi(cmd);
			
			/* ioctl call */
			ioctl(fd, AESDCHAR_IOCSEEKTO, (unsigned long)&seek_details);
			syslog(LOG_DEBUG, "IOCTL Call with %d %d", seek_details.write_cmd, seek_details.write_cmd_offset);

		} 
	    	else {	
			/* Mutex lock file writting */
			rc = pthread_mutex_lock(thread_param->writeMutex);
			if ( rc != 0 ) {
				syslog(LOG_ERR, "Mutex procurement failed with code %d\n",rc);
				return thread_func_args;
		    	}
		    		
			/* Append received data to file /var/tmp/aesdsocketdata */		
			lseek(fd, 0, SEEK_END);
			int bytesWritten = write(fd, &inBuf, bytesRead);
			/* Error writing to file */
			if (bytesWritten==-1){
				syslog(LOG_ERR, "Error: (%s) while writing to %s", strerror(errno), FILE_PATH);
				pthread_mutex_unlock(thread_param->writeMutex);
				return thread_func_args;
			} 
			/* Incomplete write to file */
			else if (bytesWritten < bytesRead){
				syslog(LOG_ERR, "Error: Incomplete write to %s, expected: %d bytes but wrote: %d bytes", FILE_PATH, bytesRead, bytesWritten);
			}
			close(fd);
			
			/* Mutex unlock file writting */
			rc = pthread_mutex_unlock(thread_param->writeMutex);
			if ( rc != 0 ) {
				syslog(LOG_ERR, "Mutex release failed with code %d\n",rc);
				return thread_func_args;
		    	}
	    	}

		
		/* If the received data contains newline, full data packet received and send back full file content */
		if (memchr(inBuf, '\n', bytesRead) != NULL){
				
			/* Mutex lock file writting */
			rc = pthread_mutex_lock(thread_param->writeMutex);
			if ( rc != 0 ) {
				syslog(LOG_ERR, "Mutex procurement failed with code %d\n",rc);
				return thread_func_args;
		    	}
		
			/* Read total data from file /var/tmp/aesdsocketdata */
			int fd = open(FILE_PATH, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
			/* Error opening file: error printed and logged and program terminates with status 1 */
			if (fd ==-1) 
			{
				syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), FILE_PATH);
				pthread_mutex_unlock(thread_param->writeMutex);
				return thread_func_args;
			}
			
			/* Reset offset to start of file for read */
			lseek(fd, 0, SEEK_SET);
			char outBuf[BUFFERSIZE];
		    				
			bytesRead = 1;
			while(bytesRead > 0){
				bytesRead = read(fd, &outBuf, BUFFERSIZE);
				int bytesSent = send(thread_param->connFd, &outBuf, bytesRead, 0);
				if (bytesSent < bytesRead) {
				    syslog(LOG_ERR, "Error sending file contents over socket connection.");
				    return thread_func_args;
				}
			}
			
			if (bytesRead < 0){
				syslog(LOG_ERR, "Error: (%s) while reading from %s", strerror(errno), FILE_PATH);
				/* Mutex unlock file writting */
				pthread_mutex_unlock(thread_param->writeMutex);
				return thread_func_args;
			} 
			
			close(fd);
			
			/* Mutex unlock file writting */
			rc = pthread_mutex_unlock(thread_param->writeMutex);
			if ( rc != 0 ) {
				syslog(LOG_ERR, "Mutex release failed with code %d\n",rc);
				return thread_func_args;
		    	}
		}
	}	
		
	close(thread_param->connFd);
	syslog(LOG_DEBUG, "Closed connection from %s", ipv4Addr);
    	
    	/* Indicate thread completed successfully */
    	thread_param->threadCompleteSuccess = true;
	return thread_func_args;
}

/* Thread function to write timestamps into file */
void* timeStampFunc(void* thread_func_args)
{
	#ifndef USE_AESD_CHAR_DEVICE
    	time_t t;
    	struct tm *tmp;
    	int rc = 0;
    
	/* Cast function arguments as thread params */
	struct timeStampThread* thread_param = (struct timeStampThread*) thread_func_args;

	while(1) {
		if(signal_caught){
		    	/* Indicate thread completed successfully */
		    	thread_param->threadCompleteSuccess = true;
			return thread_func_args;
		}
	
		sleep(TIMESTAMP_DUR);

		// Create required locals for formatted timestamp. Need to be initialized at each iteration
		char timeStamp[200] = "timestamp:";
		char outstr[200];

		//Format timestamp
		t = time(NULL);
		tmp = localtime(&t);
		if (tmp == NULL) {
			syslog(LOG_ERR, "Error getting lcoal time");
			return thread_func_args;
		}
		int timeStampBytes = strftime(outstr, sizeof(outstr), "%Y%m%d %H:%M:%S", tmp);
		if (timeStampBytes == 0) {
			syslog(LOG_ERR, "Error converting local time to string");
			return thread_func_args;
		}
	    	syslog(LOG_DEBUG, "Logging time now: %s", outstr);
		totalBytes += (timeStampBytes+11);
		strcat(timeStamp, outstr);
		strcat(timeStamp, "\n");

		/* Mutex lock file writting */
		rc = pthread_mutex_lock(thread_param->writeMutex);
		if ( rc != 0 ) {
			syslog(LOG_ERR, "Mutex procurement failed with code %d\n",rc);
			return thread_func_args;
		}

		/* Append received data to file /var/tmp/aesdsocketdata */
		int fd = open(FILE_PATH, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
		/* Error opening file: error printed and logged and program terminates with status 1 */
		if (fd ==-1) {
			syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), FILE_PATH);
			return thread_func_args;
		}
			
		lseek(fd, 0, SEEK_END);
		int bytesWritten = write(fd, &timeStamp, timeStampBytes+11);
		/* Error writing to file */
		if (bytesWritten==-1){
			syslog(LOG_ERR, "Error: (%s) while writing to %s", strerror(errno), FILE_PATH);
			return thread_func_args;
		} 
		/* Incomplete write to file */
		else if (bytesWritten < timeStampBytes+1){
			syslog(LOG_ERR, "Error: Incomplete write to %s, expected: %d bytes but wrote: %d bytes", FILE_PATH, timeStampBytes+1, bytesWritten);
		}
		close(fd);

		/* Mutex unlock file writting */
		rc = pthread_mutex_unlock(thread_param->writeMutex);
		if ( rc != 0 ) {
			syslog(LOG_ERR, "Mutex release failed with code %d\n",rc);
			return thread_func_args;
		}
	}
    	
    	/* Indicate thread completed successfully */
    	thread_param->threadCompleteSuccess = true;
	#endif
	return thread_func_args;
}

int main(int argc, char** argv){

	/*
	* Connects to system logger with no options and LOG_USER facility for user-level logging
	* NULL ident string means program name prepended to every logged message
	*/
	openlog (NULL, 0, LOG_USER); ;                       
		
	// For connection statutes
	struct addrinfo hints, *res, *addr; 
	int sockfd, status;
	int yes = 1;
	
	// For transmission of data over socket stream
	pthread_mutex_t writeMutex;
	pthread_mutex_init(&writeMutex, NULL);
	 
    	// Create singly Linked List of threads 
	SLIST_HEAD(slisthead, threadListEntry) head = SLIST_HEAD_INITIALIZER(head);;
	SLIST_INIT(&head);
	int numThreads = 0;

	memset(&hints, 0, sizeof(hints)); /* Create empty hints for populating */
	hints.ai_family = AF_INET;       /* Set IPv4 socket */
	hints.ai_socktype = SOCK_STREAM; /* Set TCP stream socket */
	hints.ai_flags = AI_PASSIVE;     /* Autofill IP */
	
	/* Get server address corresponding to port 9000 */
	status = getaddrinfo(NULL, TCP_PORT, &hints, &res);
	if (status != 0) {
		syslog(LOG_ERR, "Error getting server address with error: %s", strerror(errno));
	    	exit(-1);
	}
	
	/* Loop through all addresses found by getaddrinfo for the specified port */
	for(addr = res; addr != NULL; addr = addr->ai_next) {
	
		/* Create a socket of specified type with file descriptor */
		sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sockfd < 0){
			/* Invalid socket */
			continue;
		}
		
		/* Set socket option */
		status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if (status != 0) {
			/* Invalid configuration */
			exit(-1);
		}
		
		/* Bind socket to server address */
		status = bind(sockfd, addr->ai_addr, addr->ai_addrlen);
		if (status != 0) {
			/* Invalid bind */
			continue;
		}
		
		/* Valid socket and successful bind */
		break;
	}
	
	/* Free the address struct */
	freeaddrinfo(res); 
	
	/* Unable to bind a single socket to the server address */
	if(addr == NULL){
		syslog(LOG_ERR, "Error binding socket to server port with error: %s", strerror(errno));
	    	exit(-1);
	}
	
	/* Support -d argument which runs the aesdsocket application as a daemon */
	if (argc > 1){
		if (strcmp(argv[1], "-d") == 0){    
		    int pid = fork();
		    
		    /* Unable to spawn child process for socket */
		    if (pid < 0){
			syslog(LOG_ERR, "Error creating daemon socket with error: %s", strerror(errno));
			close(sockfd);
			exit(-1);
		    }
		    /* Close parent program to let daemon run */
		    else if (pid > 0){ 
			close(sockfd);
			exit(0);
		    }
		}
		else {
		    syslog(LOG_ERR, "Arguments Error");
		    printf("Invalid arguments used! Run the program directly with no arguments or with the -d flag.\n");
		}
	}
	
	/* 
	* Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
	* Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received
	*/
    	struct sigaction sa;
    	memset(&sa,0,sizeof(struct sigaction));
    	sa.sa_handler=signalHandler;
	if( sigaction(SIGINT, &sa, NULL) != 0 ) {
		syslog(LOG_ERR, "Error registering SIGINT with error: %s",strerror(errno));
		exit(1);
	}
	if( sigaction(SIGTERM, &sa, NULL) ) {
		syslog(LOG_ERR, "Error registering SIGTERM with error: %s",strerror(errno));
		exit(1);
	}
	
	/* With everything setup, listens for incoming connection on port 9000 */
	status = listen(sockfd, BACKLOG);
	if (status != 0) {
		syslog(LOG_ERR, "Error listening for connection with error: %s", strerror(errno));
	    	exit(-1);
	}
	
	/* Create a new thread for the connection */
	pthread_t timeStampThread;
	
	/* Initialise threadParams data struct dynamically */
	struct timeStampThread *timeStampthreadParams = (struct timeStampThread*) malloc(sizeof(struct timeStampThread));
	
	/* Populate data struct with arguments to the function */
	timeStampthreadParams->thread = &timeStampThread;		
	timeStampthreadParams->writeMutex = &writeMutex;
	timeStampthreadParams->threadCompleteSuccess = false;
	
	/* Start timestamp thread using default attributes (NULL) and threadFunc with no params */
	int rc = pthread_create(&timeStampThread,
				NULL, 
				timeStampFunc,
				timeStampthreadParams);
				 
    	/* Unsuccessful thread creation */
	if ( rc != 0 ) {
	    	free(timeStampthreadParams);
		syslog(LOG_ERR,"Creation of thread %lu failed with code %d\n",timeStampThread, rc);
	}
	
	/* Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received */
	while(1) {
	
		if(signal_caught){
			break;
		}
	
		/* Accepts incoming connection on port 9000 and stores incoming address*/
		struct sockaddr_storage incomingAddr;
	  	socklen_t addrSize = sizeof(incomingAddr);
	    	int connFd = accept(sockfd, (struct sockaddr *)&incomingAddr, &addrSize);
		if (connFd < 0){
			if(signal_caught) {
				break;
			} else {
				syslog(LOG_ERR, "Error accepting connection on socket with error: %s", strerror(errno));
				exit(-1);
			}
		}
		
		/* Create a new thread for the connection */
		pthread_t connThread;
		
		/* Initialise threadParams data struct dynamically */
		struct connThread *threadParams = (struct connThread*) malloc(sizeof(struct connThread));
		
		/* Populate data struct with arguments to the function */
		threadParams->connFd = connFd;
		threadParams->thread = connThread;		
		threadParams->incomingAddr = incomingAddr;
        	threadParams->writeMutex = &writeMutex;
        	threadParams->threadCompleteSuccess = false;
        			
		/* Add thread to linked list */
		struct threadListEntry *threadEntry = malloc(sizeof(struct threadListEntry));
		threadEntry->thread_data = threadParams;
		if(head.slh_first == NULL) {
			/* Empty list, insert at head */
			SLIST_INSERT_HEAD(&head, threadEntry, nextThread);
		}
		else {
			/* Non-empty list, insert after head */
			SLIST_INSERT_AFTER(head.slh_first, threadEntry, nextThread);
		}
		numThreads++;

        	
        	/* Start given thread using default attributes (NULL) and threadFunc with populated threadParams */
		rc = pthread_create(&connThread,
				        NULL, 
				        threadFunc,
				        threadParams);                                       
	    	
	    	/* Unsuccessful thread creation */
		if ( rc != 0 ) {
		    	free(threadParams);
		    	free(threadEntry);
			syslog(LOG_ERR,"Creation of thread %lu failed with code %d\n",connThread, rc);
		}
		
		/* Remove any finished thread from linked list and free memory*/
		struct threadListEntry *entry, *tempEntry;
		SLIST_FOREACH_SAFE(entry, &head, nextThread, tempEntry){
			if(entry->thread_data->threadCompleteSuccess) {
				syslog(LOG_DEBUG, "Trying to join thread: %ld", entry->thread_data->thread);
				pthread_join(entry->thread_data->thread,NULL);
				syslog(LOG_DEBUG, "Joined thread: %ld", entry->thread_data->thread);
				close(entry->thread_data->connFd);
				SLIST_REMOVE(&head, entry, threadListEntry, nextThread);
				numThreads--;
				free(entry->thread_data);
				free(entry);
			}
		}
	     	syslog(LOG_DEBUG, "Current threads number: %d", numThreads);
    	}
    	
    	/* Gracefully exit: completed all open connections, closed any open sockets, and deleted the file /var/tmp/aesdsocketdata. */
	struct threadListEntry *entry, *tempEntry;
 	SLIST_FOREACH_SAFE(entry, &head, nextThread, tempEntry){
		pthread_join(entry->thread_data->thread,NULL);
		close(entry->thread_data->connFd);
		SLIST_REMOVE(&head, entry, threadListEntry, nextThread);
		numThreads--;
		free(entry->thread_data);
		free(entry);
	}
 	close(sockfd);
    	
    	pthread_join(timeStampThread, NULL);
    	free(timeStampthreadParams);
    	
	/* Terminate logger connection */
	closelog ();
	
	#ifndef USE_AESD_CHAR_DEVICE
	 	status = unlink(FILE_PATH);;
	 	if (status < 0) {
			syslog(LOG_ERR, "Error: (%s) while deleting %s", strerror(errno), FILE_PATH);
			exit(1);
	 	}
 	#endif

	return 0;
}

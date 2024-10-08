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

#define TCP_PORT "9000" /* Port for socket cnnection */
#define BACKLOG 5     /* Number of pending connections in queue */
#define FILE_PATH "/var/tmp/aesdsocketdata" /* File to write data to and send data from */
#define BUFFERSIZE 500 /* Buffer for storing received data from client */

bool signal_caught = false;

/* Signal handler for SIGINT and SIGTERM,  save a copy of errno to restore later. */
static void signal_handler(int signal_number){
	int errno_saved = errno;
	signal_caught = true;
	syslog(LOG_DEBUG, "Caught signal, exiting");
    	errno = errno_saved;
}

int main(int argc, char** argv){
	/*
	* Connects to system logger with no options and LOG_USER facility for user-level logging
	* NULL ident string means program name prepended to every logged message
	*/
	openlog (NULL, 0, LOG_USER); ;                       
	
	int sockfd, status;
	int yes = 1;
	int totalBytes = 0;
	struct addrinfo hints, *res, *addr; 
	
	/* Configure server socket to be IPV4 socket transmitting data stream over protocol determined by service provider */
	//sockfd = socket(PF_INET, SOCK_STREAM, 0);  

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
    	sa.sa_handler=signal_handler;
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

	/* Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received */
	while(1) {
	
		/* Accepts incoming connection on port 9000 and stores incoming address*/
		struct sockaddr_storage incomingAddr;
	  	socklen_t addrSize = sizeof(incomingAddr);
	    	int connFd = accept(sockfd, (struct sockaddr *)&incomingAddr, &addrSize);
		if (connFd < 0){
			syslog(LOG_ERR, "Error accepting connection on socket with error: %s", strerror(errno));
			exit(-1);
		}
		
		/* Get the ip address of the connecting client */
		char ipv4Addr[INET_ADDRSTRLEN];
		struct sockaddr *incomingSockAddr = (struct sockaddr *)&incomingAddr;
	    	void *binAddr = &(((struct sockaddr_in  *)incomingSockAddr)->sin_addr);
	    	const char *res = inet_ntop(incomingAddr.ss_family, &binAddr, ipv4Addr, sizeof(ipv4Addr));
	    	if (res == NULL){
    			syslog(LOG_ERR, "Error converting client IP Address with error: %s", strerror(errno));
			exit(1);
	    	}
		syslog(LOG_DEBUG, "Accepted connection from %s", ipv4Addr);
		
		/* Receive latest data packet from client into buffer */
		int bytesRead  = 0;
		char inBuf[BUFFERSIZE];
		bool endStream = false;
		
		/* While data comes in through the stream */
		while(!endStream){
			bytesRead = recv(connFd, inBuf, sizeof(inBuf), 0);
			if(bytesRead <= 0){
				endStream = true;
				break;
			}
			totalBytes += bytesRead;
			
			/* Append received data to file /var/tmp/aesdsocketdata */
			int fd = open(FILE_PATH, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
			/* Error opening file: error printed and logged and program terminates with status 1 */
			if (fd ==-1) 
			{
				syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), FILE_PATH);
				exit(1);
			}
				
			lseek(fd, 0, SEEK_END);
			int bytesWritten = write(fd, &inBuf, bytesRead);
			/* Error writing to file */
			if (bytesWritten==-1){
				syslog(LOG_ERR, "Error: (%s) while writing to %s", strerror(errno), FILE_PATH);
				exit(1);
			} 
			/* Incomplete write to file */
			else if (bytesWritten < bytesRead){
				syslog(LOG_ERR, "Error: Incomplete write to %s, expected: %d bytes but wrote: %d bytes", FILE_PATH, bytesRead, bytesWritten);
			}
		        close(fd);

			
			/* If the received data contains newline, full data packet received and send back full file content */
			if (memchr(inBuf, '\n', bytesRead) != NULL){
				/* Read total data from file /var/tmp/aesdsocketdata */
				int fd = open(FILE_PATH, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
				/* Error opening file: error printed and logged and program terminates with status 1 */
				if (fd ==-1) 
				{
					syslog(LOG_ERR, "Error: (%s) while opening %s", strerror(errno), FILE_PATH);
					exit(1);
				}
				
				/* Reset offset to start of file for read */
				lseek(fd, 0, SEEK_SET);
				char outBuf[totalBytes];
			    				
				int bytesInFile = read(fd, &outBuf, totalBytes);
				if (bytesInFile < 0){
					syslog(LOG_ERR, "Error: (%s) while reading from %s", strerror(errno), FILE_PATH);
					exit(1);
				} 
				close(fd);
				
				int bytesSent = send(connFd, &outBuf, bytesInFile, 0);
				if (bytesSent != bytesInFile) {
				    syslog(LOG_ERR, "Error sending file contents over socket connection.");
				    exit(1);
				}
			}
		}	
		
		close(connFd);
		syslog(LOG_DEBUG, "Closed connection from %s", ipv4Addr);
		
		/* Gracefully exit: completed all open connection operations, closed any open sockets, and deleted the file /var/tmp/aesdsocketdata. */
		 if(signal_caught) {
		 	status = unlink(FILE_PATH);;
		 	if (status < 0) {
 				syslog(LOG_ERR, "Error: (%s) while deleting %s", strerror(errno), FILE_PATH);
				exit(1);
		 	}
		 	break;
		 }
    	}
    	
	/* Terminate logger connection */
	closelog ();
	
	/* Remove data file */
 	status = unlink(FILE_PATH);;
 	if (status < 0) {
		syslog(LOG_ERR, "Error: (%s) while deleting %s", strerror(errno), FILE_PATH);
		exit(1);
 	}
 	close(sockfd);
	return 0;
}


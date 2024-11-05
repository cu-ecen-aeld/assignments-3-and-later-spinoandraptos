#include <stdbool.h>
#include <pthread.h>

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct connThread {
	
	/* Socket fd identifier for the connection */
    	int connFd ; 
    	
    	/* Created thread */
    	pthread_t thread;
    	
    	/* Address of connecting client */
    	struct sockaddr_storage incomingAddr;
    	
	/* Pointer to mutex */
	pthread_mutex_t* writeMutex;

	/* True if the thread completed with success, false if an error occurred. */
	bool threadCompleteSuccess;
};

struct timeStampThread {
	
    	/* Pointer to created thread */
    	pthread_t *thread;
    	
	/* Pointer to mutex */
	pthread_mutex_t* writeMutex;

	/* True if the thread completed with success, false if an error occurred. */
	bool threadCompleteSuccess;
};

struct threadListEntry {
	
	/* Params of thread */
	struct connThread *thread_data;
	
	/* Next thread in the singly linked list */
	SLIST_ENTRY(threadListEntry) nextThread;
};



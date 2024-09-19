#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_func_args)
{	
	/* Cast function arguments as thread params */
	struct thread_data* thread_param = (struct thread_data*) thread_func_args;
	
	/* Wait before obtaining mutex */
	usleep(thread_param->wait_mutex_time * 1000);
	
    	/* Obtains the mutex and locks it */
	int rc = pthread_mutex_lock(thread_param->mutex);
	if ( rc != 0 ) {
		thread_param->thread_complete_success = false;
		printf("Mutex procurement failed with code %d\n",rc);
    	}
    	
	/* Wait before releasing mutex */
	usleep(thread_param->lock_mutex_time * 1000);
	
    	/* Releases the mutex */
	rc = pthread_mutex_unlock(thread_param->mutex);
	if ( rc != 0 ) {
		thread_param->thread_complete_success = false;
		printf("Mutex release failed with code %d\n",rc);
    	}
    	
    	/* Indicate thread completed successfully */
    	thread_param->thread_complete_success = true;
	return thread_func_args;
}

/**
* Start a thread which sleeps @param wait_to_obtain_ms number of milliseconds, then obtains the
* mutex in @param mutex, then holds for @param wait_to_release_ms milliseconds, then releases.
* The start_thread_obtaining_mutex function should only start the thread and should not block
* for the thread to complete.
* The start_thread_obtaining_mutex function should use dynamic memory allocation for thread_data
* structure passed into the thread.  The number of threads active should be limited only by the
* amount of available memory.
* The thread started should return a pointer to the thread_data structure when it exits, which can be used
* to free memory as well as to check thread_complete_success for successful exit.
* If a thread was started succesfully @param thread should be filled with the pthread_create thread ID
* coresponding to the thread which was started.
* @return true if the thread could be started, false if a failure occurred.
*/
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	/* Initialise thread_params data struct */
	struct thread_data *thread_params;
	
	/* Dynamically allocate memory to thread_params */
	thread_params = (struct thread_data*) malloc(sizeof(struct thread_data));
	
	/* Populate data struct with arguments to the function */
        thread_params->mutex=mutex;
        thread_params->wait_mutex_time=wait_to_obtain_ms;
        thread_params->lock_mutex_time=wait_to_release_ms;
        thread_params->thread_complete_success=false;
        
        /* Start given thread using default attributes (NULL) and threadfunc with populated thread_params */
	int rc = pthread_create(thread,
		                NULL, 
		                threadfunc,
		                thread_params);                                       
    	
    	/* Unsuccessful thread creation */
	if ( rc != 0 ) {
	    	free(thread_params);
		printf("Creation of thread %ln failed with code %d\n",thread, rc);
	   	return false;
   	/* Successful thread creation */
	} else {
		return true;
	}
}


#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int s = usleep(thread_func_args->wait_to_obtain_ms * 1000);
    if (s != 0) {
        ERROR_LOG("Error sleeping to obtain mutex");
        thread_func_args->thread_complete_success = false;
    }
    s = pthread_mutex_lock(thread_func_args->mutex);
    if (s != 0) {
        ERROR_LOG("Error obtaining mutex");
        thread_func_args->thread_complete_success = false;
    }
    s = usleep(thread_func_args->wait_to_release_ms * 1000);
    if (s != 0) {
        ERROR_LOG("Error sleeping with mutex");
        thread_func_args->thread_complete_success = false;
    }
    s = pthread_mutex_unlock(thread_func_args->mutex);
    if (s != 0) {
        ERROR_LOG("Error releasing mutex");
        thread_func_args->thread_complete_success = false;
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *arguments = malloc(sizeof(struct thread_data));
    if (arguments == NULL) {
        ERROR_LOG("Error Allocating memory");
        return false;
    }
    arguments->wait_to_obtain_ms = wait_to_obtain_ms;
    arguments->wait_to_release_ms = wait_to_release_ms;
    arguments->thread_complete_success = true;
    arguments->mutex = mutex;

    int s = pthread_create(thread, NULL, threadfunc, (void*)arguments);
    if (s != 0) {
        ERROR_LOG("Error creating thread");
        free(arguments);
        return false;
    }

    void* void_data;
    s = pthread_join(*thread, &void_data);
    if (s != 0) {
        ERROR_LOG("Error joining thread");
        free(arguments);
        return false;
    }
    struct thread_data* data_returned = (struct thread_data*) void_data;

    bool toReturn = data_returned->thread_complete_success;

    free(data_returned);
    free(arguments);

    return toReturn;
}


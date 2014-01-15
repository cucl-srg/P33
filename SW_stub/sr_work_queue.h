/*
 * Filename: sr_work_queue.h
 * Purpose: Defines a thread-safe work queue.
 */

#ifndef _THREAD_PER_PACKET_
#ifndef SR_WORK_QUEUE_H
#define SR_WORK_QUEUE_H

#include <pthread.h>
#include "sr_common.h"

/** encapsulates a single piece of work */
typedef struct work_t {
    int type;
    void* work;

    struct work_t* next; /* internal use only */
} work_t;

/** defines a thread-safe work queue */
typedef struct work_queue_t {
    pthread_mutex_t lock;
    pthread_cond_t  cond_work; /* a thread waiting on this will be notified when
                                  work is added to the work queue */

    void (*func_do_work)(work_t*); /* function to call to do the work */
    work_t* head;
    work_t* tail;
    bool done;
    unsigned workers;
} work_queue_t;

/**
 * Creates a new work queue with a pool of num_workers worker threads which will
 * be dedicated to getting work off the queue.
 *
 * @param num_workers   number of worker threads to spawn (>=1)
 * @param func_do_work  workers will call this function to process work
 */
void wq_init( work_queue_t* wq,
              unsigned num_workers,
              void (*func_do_work)(work_t* /* borrowed */) );

/**
 * Destroys an existing work queue.  Outstanding jobs will be deleted.  Threads
 * which are currently running a job will terminate as soon as they finish their
 * current job.  All idle threads will terminate immediately.  This function
 * gives the workers some time to terminate and then returns.
 */
void wq_destroy( work_queue_t* wq );

/**
 * Adds new work to the work queue.  If there are any threads waiting for work,
 * then one of those is notified that work is now available.  The work queue
 * will free the work argument when it has been completed.
 */
void wq_enqueue( work_queue_t* wq, int type, void* work );

/**
 * The caller will get work from the queue to handle, or go to sleep until work
 * is available.
 */
void wq_wait_for_work( work_queue_t* wq );

#endif /* SR_WORK_QUEUE_H */
#endif /* _THREAD_PER_PACKET_ */

/* Filename: sr_work_queue.c */

#ifndef _THREAD_PER_PACKET_

#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* malloc, free */
#include <unistd.h> /* sleep */
#include "sr_thread.h"
#include "sr_router.h"
#include "sr_work_queue.h"

/**
 * Entry point for a new pthread worker thread for the work queue which is
 * passed as the wq argument.  The thread detaches and then calls
 * wq_wait_for_work().
 */
THREAD_RETURN_TYPE wq_pthread_main( void* wq );

void wq_init( work_queue_t* wq,
              unsigned num_workers,
              void (*func_do_work)(work_t*) ) {
    pthread_mutex_init( &wq->lock,      NULL );
    pthread_cond_init(  &wq->cond_work, NULL );

    wq->done = FALSE;
    wq->tail = NULL;
    wq->head = NULL;
    wq->workers = num_workers;
    wq->func_do_work = func_do_work;

    /* spawn the workers */
    true_or_die( num_workers, "Error: The work queue must have at least one worker" );
    while( num_workers-- > 0 )
        make_thread( wq_pthread_main, wq );
}

void wq_destroy( work_queue_t* wq ) {
    unsigned time_left;
    work_t* tmp;

    pthread_mutex_lock( &wq->lock );

    /* tell the worker threads to terminate */
    wq->done = TRUE;

    /* clean up remaining work */
    if( wq->head )
        debug_println( "Destroying work left on the queue for wq_destroy ..." );

    while( wq->head ) {
        tmp = wq->head;
        wq->head = wq->head->next;
        myfree( tmp );
    }

    pthread_mutex_unlock( &wq->lock );
    pthread_cond_broadcast( &wq->cond_work );

    /* give the workers some time to shutdown */
    time_left = SHUTDOWN_TIME;

    /* the race here with wq->workers is benign */
    while( time_left-- > 0 && wq->workers>0 ) {
        debug_println( "Waiting for workers to shutdown (%u left) ...",
                       wq->workers );
        sleep( 1 );
    }

    pthread_mutex_destroy( &wq->lock );
    pthread_cond_destroy( &wq->cond_work );
}

void wq_enqueue( work_queue_t* wq, int type, void* work ) {
    work_t* w;

    pthread_mutex_lock( &wq->lock );

    w = malloc_or_die( sizeof(*w) );

    w->type = type;
    w->work = work;
    w->next = NULL;

    /* put the work on the end of the queue */
    if( wq->tail )
        wq->tail->next = w;
    else
        wq->head = w;

    wq->tail = w;

    pthread_mutex_unlock( &wq->lock );

    /* tell a waiting thread (if any) that work is available */
    pthread_cond_signal( &wq->cond_work );
}

THREAD_RETURN_TYPE wq_pthread_main( void* wq ) {
    static unsigned id = 0;
    char name[10];
    snprintf( name, 10, "Worker %u", id++ );
    debug_pthread_init( name, "Worker Thread" );
    pthread_detach( pthread_self() );
    wq_wait_for_work( (work_queue_t*)wq );
    THREAD_RETURN_NIL;
}

void wq_wait_for_work( work_queue_t* wq ) {
    work_t* work;

    /* do work! */
    while( 1) {
        pthread_mutex_lock( &wq->lock );

        /* wait for work (releases the lock and will-reaquire on wait's end) */
        while( !wq->done && !wq->head )
            pthread_cond_wait( &wq->cond_work, &wq->lock );

        /* stop if work is being halted */
        if( wq->done ) {
            debug_println( "Worker Thread shutting down" );
            wq->workers -= 1;
            pthread_mutex_unlock( &wq->lock );
            return;
        }

        /* get the next piece of work */
        true_or_die( wq->head != NULL, "Error: no work on the work queue?" );
        debug_println( "Worker got some work to do" );
        work = wq->head;
        wq->head = wq->head->next;
        if( wq->head == NULL )
            wq->tail = NULL;

        /* give up the lock */
        pthread_mutex_unlock( &wq->lock );

        /* process the work */
        wq->func_do_work( work );

        /* cleanup the work object */
        myfree( work );
    }
}

#endif /* _THREAD_PER_PACKET_ */

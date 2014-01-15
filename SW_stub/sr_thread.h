/*
 * Filename: sr_thread.h
 * Purpose: setup gross threading includes for LWTCP threads
 */

#include <pthread.h>
#include "lwip/sys.h"

#ifdef _STANDALONE_CLI_
#   define THREAD_RETURN_NIL return NULL
#   define THREAD_RETURN_TYPE void*
#   define make_thread(func,arg) { \
        pthread_t tid;                                      \
        true_or_die( pthread_create(&tid,NULL,func,arg)==0, \
                     "pthread_create failed" );             \
    }
#else
#   define THREAD_RETURN_NIL return
#   define THREAD_RETURN_TYPE void
#   define make_thread(func,arg) sys_thread_new(func,arg);
#endif

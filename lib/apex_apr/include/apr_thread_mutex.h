#ifndef APEX_APR_APR_THREAD_MUTEX_H
#define APEX_APR_APR_THREAD_MUTEX_H

#include_next <apr_thread_mutex.h>

#ifdef USE_APEX_API

/*
 * Future APEX-backed mutex skeleton.
 */
typedef struct apex_apr_thread_mutex_layout {
	void *native_mutex;
	void *owner;
	void *policy_state;
} apex_apr_thread_mutex_layout;

/*
 * apr_thread_mutex_t is treated as an opaque synchronization handle during the
 * transition. The future APEX-backed representation can replace the aliased
 * APR handle once the mutex contract is finalized.
 */
typedef apr_thread_mutex_t apex_apr_thread_mutex_t;
#define apr_thread_mutex_t apex_apr_thread_mutex_t

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_MUTEX_H */

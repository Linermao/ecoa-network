#ifndef APEX_APR_APR_THREAD_COND_H
#define APEX_APR_APR_THREAD_COND_H

#include_next <apr_thread_cond.h>

#ifdef USE_APEX_API

/*
 * Future APEX-backed condition/event skeleton.
 */
typedef struct apex_apr_thread_cond_layout {
	void *native_condition;
	void *wait_queue_state;
	void *policy_state;
} apex_apr_thread_cond_layout;

/*
 * apr_thread_cond_t remains an opaque synchronization handle in the shim.
 * Its eventual APEX mapping can be an event/semaphore composite without
 * changing call sites first.
 */
typedef apr_thread_cond_t apex_apr_thread_cond_t;
#define apr_thread_cond_t apex_apr_thread_cond_t

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_COND_H */

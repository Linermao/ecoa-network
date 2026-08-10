#ifndef APEX_APR_APR_THREAD_COND_H
#define APEX_APR_APR_THREAD_COND_H

#include_next <apr_thread_cond.h>

#include "apex_apr_size.h"

#ifdef USE_APEX_API

#include <apr_thread_mutex.h>

#include <a653Semaphore.h>

#ifndef INFINITE_TIME_VALUE
#define INFINITE_TIME_VALUE ((SYSTEM_TIME_TYPE)-1)
#endif

typedef struct apex_apr_thread_cond_t {
	SEMAPHORE_ID_TYPE semaphore_id;
	unsigned int waiters;
	unsigned int pending_signals;
	apr_pool_t *pool;
	int active;
} apex_apr_thread_cond_t;

#define apr_thread_cond_t apex_apr_thread_cond_t

apr_status_t apex_apr_thread_cond_create(apr_thread_cond_t **cond,
								 apr_pool_t *pool);
apr_status_t apex_apr_thread_cond_wait(apr_thread_cond_t *cond,
							   apr_thread_mutex_t *mutex);
apr_status_t apex_apr_thread_cond_timedwait(apr_thread_cond_t *cond,
									apr_thread_mutex_t *mutex,
									apr_interval_time_t timeout);
apr_status_t apex_apr_thread_cond_signal(apr_thread_cond_t *cond);
apr_status_t apex_apr_thread_cond_broadcast(apr_thread_cond_t *cond);
apr_status_t apex_apr_thread_cond_destroy(apr_thread_cond_t *cond);
apr_pool_t *apex_apr_thread_cond_pool_get(const apr_thread_cond_t *cond);

#define apr_thread_cond_create apex_apr_thread_cond_create
#define apr_thread_cond_wait apex_apr_thread_cond_wait
#define apr_thread_cond_timedwait apex_apr_thread_cond_timedwait
#define apr_thread_cond_signal apex_apr_thread_cond_signal
#define apr_thread_cond_broadcast apex_apr_thread_cond_broadcast
#define apr_thread_cond_destroy apex_apr_thread_cond_destroy
#define apr_thread_cond_pool_get apex_apr_thread_cond_pool_get

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_COND_H */

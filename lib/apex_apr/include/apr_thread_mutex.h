#ifndef APEX_APR_APR_THREAD_MUTEX_H
#define APEX_APR_APR_THREAD_MUTEX_H

#include_next <apr_thread_mutex.h>

#include <apex_apr_size.h>

#ifdef USE_APEX_API

/* Re-enter the pools shim after APR's native header include chain. */
#include <apr_pools.h>
#include <a653Mutex.h>

#ifndef INFINITE_TIME_VALUE
#define INFINITE_TIME_VALUE ((SYSTEM_TIME_TYPE)-1)
#endif

typedef struct apex_apr_thread_mutex_t {
	MUTEX_ID_TYPE mutex_id;
	unsigned int flags;
	apr_pool_t *pool;
	int active;
} apex_apr_thread_mutex_t;

#define apr_thread_mutex_t apex_apr_thread_mutex_t

apr_status_t apex_apr_thread_mutex_create(apr_thread_mutex_t **mutex,
								  unsigned int flags,
								  apr_pool_t *pool);
apr_status_t apex_apr_thread_mutex_lock(apr_thread_mutex_t *mutex);
apr_status_t apex_apr_thread_mutex_trylock(apr_thread_mutex_t *mutex);
apr_status_t apex_apr_thread_mutex_timedlock(apr_thread_mutex_t *mutex,
								     apr_interval_time_t timeout);
apr_status_t apex_apr_thread_mutex_unlock(apr_thread_mutex_t *mutex);
apr_status_t apex_apr_thread_mutex_destroy(apr_thread_mutex_t *mutex);
apr_pool_t *apex_apr_thread_mutex_pool_get(const apr_thread_mutex_t *mutex);

#define apr_thread_mutex_create apex_apr_thread_mutex_create
#define apr_thread_mutex_lock apex_apr_thread_mutex_lock
#define apr_thread_mutex_trylock apex_apr_thread_mutex_trylock
#define apr_thread_mutex_timedlock apex_apr_thread_mutex_timedlock
#define apr_thread_mutex_unlock apex_apr_thread_mutex_unlock
#define apr_thread_mutex_destroy apex_apr_thread_mutex_destroy
#define apr_thread_mutex_pool_get apex_apr_thread_mutex_pool_get

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_MUTEX_H */

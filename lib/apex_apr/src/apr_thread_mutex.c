#include <stdio.h>
#include <stdint.h>

#include <apr_thread_mutex.h>

#ifdef USE_APEX_API
#undef apr_thread_mutex_create
#undef apr_thread_mutex_lock
#undef apr_thread_mutex_trylock
#undef apr_thread_mutex_timedlock
#undef apr_thread_mutex_unlock
#undef apr_thread_mutex_destroy
#undef apr_thread_mutex_pool_get

#define APEX_APR_MUTEX_PRIORITY 1

static unsigned int apex_apr_mutex_sequence;

static apr_status_t apex_apr_mutex_status_to_apr(RETURN_CODE_TYPE return_code)
{
	if (return_code == NO_ERROR) {
		return APR_SUCCESS;
	}
	if (return_code == TIMED_OUT) {
		return APR_TIMEUP;
	}
	if (return_code == NOT_AVAILABLE || return_code == NO_ACTION) {
		return APR_EBUSY;
	}
	if (return_code == INVALID_PARAM) {
		return APR_EINVAL;
	}
	return APR_EGENERAL;
}

static apr_status_t apex_apr_mutex_timeout_from_usec(
	apr_interval_time_t timeout, SYSTEM_TIME_TYPE *apex_timeout)
{
	if (apex_timeout == NULL || timeout < 0 ||
	    (uint64_t)timeout > INT64_MAX / 1000u) {
		return APR_EINVAL;
	}
	*apex_timeout = (SYSTEM_TIME_TYPE)((uint64_t)timeout * 1000u);
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_mutex_create(apr_thread_mutex_t **mutex,
								  unsigned int flags,
								  apr_pool_t *pool)
{
	apr_thread_mutex_t *created;
	MUTEX_NAME_TYPE name;
	RETURN_CODE_TYPE return_code;

	if (mutex == NULL || pool == NULL ||
	    (flags & ~(APR_THREAD_MUTEX_NESTED | APR_THREAD_MUTEX_UNNESTED |
		       APR_THREAD_MUTEX_TIMED)) != 0 ||
	    ((flags & APR_THREAD_MUTEX_NESTED) != 0 &&
	     (flags & APR_THREAD_MUTEX_UNNESTED) != 0)) {
		return APR_EINVAL;
	}

	created = apr_pcalloc(pool, sizeof(*created));
	if (created == NULL) {
		return APR_ENOMEM;
	}
	(void)snprintf(name, sizeof(name), "APRMTX%u", ++apex_apr_mutex_sequence);
	CREATE_MUTEX(name, APEX_APR_MUTEX_PRIORITY, FIFO,
		     &created->mutex_id, &return_code);
	if (return_code != NO_ERROR) {
		return apex_apr_mutex_status_to_apr(return_code);
	}
	created->flags = flags;
	created->pool = pool;
	created->active = 1;
	*mutex = created;
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_mutex_lock(apr_thread_mutex_t *mutex)
{
	RETURN_CODE_TYPE return_code;

	if (mutex == NULL || !mutex->active) {
		return APR_EINVAL;
	}
	ACQUIRE_MUTEX(mutex->mutex_id, INFINITE_TIME_VALUE, &return_code);
	return apex_apr_mutex_status_to_apr(return_code);
}

apr_status_t apex_apr_thread_mutex_trylock(apr_thread_mutex_t *mutex)
{
	RETURN_CODE_TYPE return_code;
	apr_status_t status;

	if (mutex == NULL || !mutex->active) {
		return APR_EINVAL;
	}
	ACQUIRE_MUTEX(mutex->mutex_id, 0, &return_code);
	status = apex_apr_mutex_status_to_apr(return_code);
	return status == APR_TIMEUP ? APR_EBUSY : status;
}

apr_status_t apex_apr_thread_mutex_timedlock(apr_thread_mutex_t *mutex,
								     apr_interval_time_t timeout)
{
	RETURN_CODE_TYPE return_code;
	SYSTEM_TIME_TYPE apex_timeout;
	apr_status_t status;

	if (mutex == NULL || !mutex->active) {
		return APR_EINVAL;
	}
	if (timeout <= 0) {
		status = apex_apr_thread_mutex_trylock(mutex);
		return status == APR_EBUSY ? APR_TIMEUP : status;
	}
	status = apex_apr_mutex_timeout_from_usec(timeout, &apex_timeout);
	if (status != APR_SUCCESS) {
		return status;
	}
	ACQUIRE_MUTEX(mutex->mutex_id, apex_timeout, &return_code);
	status = apex_apr_mutex_status_to_apr(return_code);
	return status == APR_EBUSY ? APR_TIMEUP : status;
}

apr_status_t apex_apr_thread_mutex_unlock(apr_thread_mutex_t *mutex)
{
	RETURN_CODE_TYPE return_code;

	if (mutex == NULL || !mutex->active) {
		return APR_EINVAL;
	}
	RELEASE_MUTEX(mutex->mutex_id, &return_code);
	return apex_apr_mutex_status_to_apr(return_code);
}

apr_status_t apex_apr_thread_mutex_destroy(apr_thread_mutex_t *mutex)
{
	if (mutex == NULL || !mutex->active) {
		return APR_EINVAL;
	}
	/* Standard APEX exposes no mutex delete service. */
	mutex->active = 0;
	return APR_SUCCESS;
}

apr_pool_t *apex_apr_thread_mutex_pool_get(const apr_thread_mutex_t *mutex)
{
	return mutex != NULL && mutex->active ? mutex->pool : NULL;
}

#endif /* USE_APEX_API */

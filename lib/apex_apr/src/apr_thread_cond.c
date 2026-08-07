#include <stdio.h>
#include <stdint.h>

#include <apr_thread_cond.h>
#include <apr_thread_mutex.h>

#ifdef USE_APEX_API
#undef apr_thread_cond_create
#undef apr_thread_cond_wait
#undef apr_thread_cond_timedwait
#undef apr_thread_cond_signal
#undef apr_thread_cond_broadcast
#undef apr_thread_cond_destroy
#undef apr_thread_cond_pool_get

#define APEX_APR_COND_MAX_WAITERS 64

static unsigned int apex_apr_cond_sequence;

static apr_status_t apex_apr_cond_status_to_apr(RETURN_CODE_TYPE return_code)
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

static apr_status_t apex_apr_cond_timeout_from_usec(
	apr_interval_time_t timeout, SYSTEM_TIME_TYPE *apex_timeout)
{
	if (apex_timeout == NULL || timeout < 0 ||
	    (uint64_t)timeout > INT64_MAX / 1000u) {
		return APR_EINVAL;
	}
	*apex_timeout = (SYSTEM_TIME_TYPE)((uint64_t)timeout * 1000u);
	return APR_SUCCESS;
}

static apr_status_t apex_apr_cond_wait_internal(
	apr_thread_cond_t *cond, apr_thread_mutex_t *mutex,
	apr_interval_time_t timeout, int timed)
{
	RETURN_CODE_TYPE return_code;
	SYSTEM_TIME_TYPE apex_timeout;
	apr_status_t status;

	if (cond == NULL || mutex == NULL || !cond->active || !mutex->active) {
		return APR_EINVAL;
	}
	if (cond->waiters >= APEX_APR_COND_MAX_WAITERS) {
		return APR_ENOMEM;
	}

	/* The caller-held mutex serializes waiter and pending-signal accounting. */
	++cond->waiters;
	status = apex_apr_thread_mutex_unlock(mutex);
	if (status != APR_SUCCESS) {
		--cond->waiters;
		return status;
	}

	if (!timed) {
		WAIT_SEMAPHORE(cond->semaphore_id, INFINITE_TIME_VALUE, &return_code);
	} else if (timeout <= 0) {
		WAIT_SEMAPHORE(cond->semaphore_id, 0, &return_code);
	} else {
		status = apex_apr_cond_timeout_from_usec(timeout, &apex_timeout);
		if (status != APR_SUCCESS) {
			(void)apex_apr_thread_mutex_lock(mutex);
			--cond->waiters;
			return status;
		}
		WAIT_SEMAPHORE(cond->semaphore_id, apex_timeout, &return_code);
	}

	status = apex_apr_thread_mutex_lock(mutex);
	if (status != APR_SUCCESS) {
		return status;
	}

	--cond->waiters;
	if (return_code == NO_ERROR) {
		if (cond->pending_signals > 0) {
			--cond->pending_signals;
		}
		return APR_SUCCESS;
	}

	/* Remove a token left by a signal that raced this timed wait. */
	if (cond->pending_signals > cond->waiters) {
		RETURN_CODE_TYPE discard_return_code;

		WAIT_SEMAPHORE(cond->semaphore_id, 0, &discard_return_code);
		if (discard_return_code == NO_ERROR) {
			--cond->pending_signals;
		}
	}
	status = apex_apr_cond_status_to_apr(return_code);
	return status == APR_EBUSY ? APR_TIMEUP : status;
}

apr_status_t apex_apr_thread_cond_create(apr_thread_cond_t **cond, apr_pool_t *pool)
{
	apr_thread_cond_t *created;
	SEMAPHORE_NAME_TYPE name;
	RETURN_CODE_TYPE return_code;

	if (cond == NULL || pool == NULL) {
		return APR_EINVAL;
	}
	created = apr_pcalloc(pool, sizeof(*created));
	if (created == NULL) {
		return APR_ENOMEM;
	}
	(void)snprintf(name, sizeof(name), "APRCON%u", ++apex_apr_cond_sequence);
	CREATE_SEMAPHORE(name, 0, APEX_APR_COND_MAX_WAITERS, FIFO,
			 &created->semaphore_id, &return_code);
	if (return_code != NO_ERROR) {
		return apex_apr_cond_status_to_apr(return_code);
	}
	created->pool = pool;
	created->active = 1;
	*cond = created;
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_cond_wait(apr_thread_cond_t *cond,
							   apr_thread_mutex_t *mutex)
{
	return apex_apr_cond_wait_internal(cond, mutex, 0, 0);
}

apr_status_t apex_apr_thread_cond_timedwait(apr_thread_cond_t *cond,
									apr_thread_mutex_t *mutex,
									apr_interval_time_t timeout)
{
	return apex_apr_cond_wait_internal(cond, mutex, timeout, 1);
}

apr_status_t apex_apr_thread_cond_signal(apr_thread_cond_t *cond)
{
	RETURN_CODE_TYPE return_code;

	if (cond == NULL || !cond->active) {
		return APR_EINVAL;
	}
	if (cond->waiters <= cond->pending_signals) {
		return APR_SUCCESS;
	}
	SIGNAL_SEMAPHORE(cond->semaphore_id, &return_code);
	if (return_code != NO_ERROR) {
		return apex_apr_cond_status_to_apr(return_code);
	}
	++cond->pending_signals;
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_cond_broadcast(apr_thread_cond_t *cond)
{
	RETURN_CODE_TYPE return_code;
	unsigned int signals;
	unsigned int i;

	if (cond == NULL || !cond->active) {
		return APR_EINVAL;
	}
	signals = cond->waiters - cond->pending_signals;
	for (i = 0; i < signals; ++i) {
		SIGNAL_SEMAPHORE(cond->semaphore_id, &return_code);
		if (return_code != NO_ERROR) {
			return apex_apr_cond_status_to_apr(return_code);
		}
	}
	cond->pending_signals += signals;
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_cond_destroy(apr_thread_cond_t *cond)
{
	if (cond == NULL || !cond->active) {
		return APR_EINVAL;
	}
	if (cond->waiters != 0) {
		return APR_EBUSY;
	}
	/* Standard APEX exposes no semaphore delete service. */
	cond->active = 0;
	return APR_SUCCESS;
}

apr_pool_t *apex_apr_thread_cond_pool_get(const apr_thread_cond_t *cond)
{
	return cond != NULL && cond->active ? cond->pool : NULL;
}

#endif /* USE_APEX_API */

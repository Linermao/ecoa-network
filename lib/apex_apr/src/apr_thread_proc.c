/*
 * APR thread/process shim.
 *
 * This first stage proves that LDP thread calls are intercepted when
 * USE_APEX_API is enabled.  The APEX pool shim is not binary-compatible with
 * a native APR pool, so delegating these calls to system APR would be unsafe.
 * Until the target SDK's PROCESS lifecycle and join-equivalent semantics are
 * implemented, the wrappers fail explicitly instead.
 */

#include <apr_thread_proc.h>

#ifdef USE_APEX_API
#undef apr_threadattr_create
#undef apr_thread_create
#undef apr_thread_join
#undef apr_thread_pool_get

apr_status_t apex_apr_threadattr_create(apr_threadattr_t **new_attr,
                                        apr_pool_t *pool)
{
	if (new_attr != NULL) {
		*new_attr = NULL;
	}
	(void)pool;
	return APR_ENOTIMPL;
}

apr_status_t apex_apr_thread_create(apr_thread_t **new_thread,
                                    apr_threadattr_t *attr,
                                    apr_thread_start_t func,
                                    void *data,
                                    apr_pool_t *pool)
{
	if (new_thread != NULL) {
		*new_thread = NULL;
	}
	(void)attr;
	(void)func;
	(void)data;
	(void)pool;
	return APR_ENOTIMPL;
}

apr_status_t apex_apr_thread_join(apr_status_t *retval,
                                  apr_thread_t *thread)
{
	if (retval != NULL) {
		*retval = APR_ENOTIMPL;
	}
	(void)thread;
	return APR_ENOTIMPL;
}

apr_pool_t *apex_apr_thread_pool_get(const apr_thread_t *thread)
{
	(void)thread;
	return NULL;
}

#endif /* USE_APEX_API */

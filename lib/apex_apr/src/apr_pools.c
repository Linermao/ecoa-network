#include <stdio.h>
#include <string.h>

#include <apr_pools.h>

#ifdef USE_APEX_API
#undef apr_pool_create
#undef apr_pool_destroy
#undef apr_palloc
#undef apr_pcalloc

/*
 * Pool wrappers stay grouped in this module so allocator migration can happen
 * as one audited unit instead of being scattered across unrelated files.
 * apr_pool_t itself still bridges to native APR for now; the future custom
 * layout is documented in apr_pools.h as apex_apr_pool_layout.
 */
apr_status_t apex_apr_pool_create(apr_pool_t **newpool, apr_pool_t *parent)
{
	printf("[apex_apr] apr_pool_create(%p, %p) intercepted, delegating to system APR\n",
	       (void *)newpool,
	       (void *)parent);
	fflush(stdout);
	return apr_pool_create_ex(newpool, parent, NULL, NULL);
}

void apex_apr_pool_destroy(apr_pool_t *p)
{
	printf("[apex_apr] apr_pool_destroy(%p) intercepted, delegating to system APR\n",
	       (void *)p);
	fflush(stdout);
#if APR_POOL_DEBUG
	apr_pool_destroy_debug(p, APR_POOL__FILE_LINE__);
#else
	apr_pool_destroy(p);
#endif
}

void *apex_apr_palloc(apr_pool_t *p, apr_size_t size)
{
	printf("[apex_apr] apr_palloc(%p, %lu) intercepted, delegating to system APR\n",
	       (void *)p,
	       (unsigned long)size);
	fflush(stdout);
#if APR_POOL_DEBUG
	return apr_palloc_debug(p, size, APR_POOL__FILE_LINE__);
#else
	return apr_palloc(p, size);
#endif
}

void *apex_apr_pcalloc(apr_pool_t *p, apr_size_t size)
{
	void *mem;

	printf("[apex_apr] apr_pcalloc(%p, %lu) intercepted, delegating to system APR\n",
	       (void *)p,
	       (unsigned long)size);
	fflush(stdout);
#if APR_POOL_DEBUG
	return apr_pcalloc_debug(p, size, APR_POOL__FILE_LINE__);
#else
	mem = apr_palloc(p, size);
	if (mem != NULL) {
		memset(mem, 0, size);
	}
	return mem;
#endif
}

#endif /* USE_APEX_API */

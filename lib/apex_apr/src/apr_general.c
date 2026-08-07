#include <apr_general.h>
#include <apr_pools.h>

#ifdef USE_APEX_API
#undef apr_initialize
#undef apr_terminate

/* Global setup happens before application tasks create APEX APR resources. */
static unsigned int apex_apr_initialization_count;

apr_status_t apex_apr_initialize(void)
{
	apr_status_t status;

	if (apex_apr_initialization_count != 0) {
		++apex_apr_initialization_count;
		return APR_SUCCESS;
	}

	status = apex_apr_pool_initialize();
	if (status != APR_SUCCESS) {
		return status;
	}

	apex_apr_initialization_count = 1;
	return APR_SUCCESS;
}

void apex_apr_terminate(void)
{
	if (apex_apr_initialization_count == 0) {
		return;
	}

	--apex_apr_initialization_count;
	if (apex_apr_initialization_count == 0) {
		apex_apr_pool_terminate();
	}
}

#endif /* USE_APEX_API */

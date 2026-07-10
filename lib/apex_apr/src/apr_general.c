#include <stdio.h>

#include <apr_general.h>

#ifdef USE_APEX_API
#undef apr_initialize
#undef apr_terminate

/*
 * Keep lifecycle wrappers in delegate mode until the APEX runtime contract is
 * defined. That lets the build prove interception without changing ownership
 * rules for APR global state yet.
 */
apr_status_t apex_apr_initialize(void)
{
	printf("[apex_apr] apr_initialize() intercepted, delegating to system APR\n");
	fflush(stdout);
	return apr_initialize();
}

void apex_apr_terminate(void)
{
	printf("[apex_apr] apr_terminate() intercepted, delegating to system APR\n");
	fflush(stdout);
	apr_terminate();
}

#endif /* USE_APEX_API */

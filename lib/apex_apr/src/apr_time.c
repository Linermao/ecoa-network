#include <stdio.h>

#include <apr_time.h>

#ifdef USE_APEX_API
#undef apr_sleep

void apex_apr_sleep(apr_interval_time_t t)
{
	printf("[apex_apr] apr_sleep(%ld) intercepted, delegating to system APR\n", (long)t);
	fflush(stdout);
	apr_sleep(t);
}

#endif /* USE_APEX_API */

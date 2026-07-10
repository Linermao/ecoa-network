#include <stdio.h>

#include <apr_time.h>

#ifdef USE_APEX_API
#undef apr_sleep
#undef apr_time_now
#undef apr_time_from_sec
#undef apr_time_usec

/*
 * apr_time_now/apr_sleep keep delegating to APR because they depend on the
 * platform clock, while apr_time_from_sec/apr_time_usec are lifted into local
 * compatibility math because upstream APR exposes them as macros.
 */
void apex_apr_sleep(apr_interval_time_t t)
{
	printf("[apex_apr] apr_sleep(%ld) intercepted, delegating to system APR\n", (long)t);
	fflush(stdout);
	apr_sleep(t);
}

apr_time_t apex_apr_time_now(void)
{
	printf("[apex_apr] apr_time_now() intercepted, delegating to system APR\n");
	fflush(stdout);
	return apr_time_now();
}

apr_time_t apex_apr_time_from_sec(apr_time_t sec)
{
	printf("[apex_apr] apr_time_from_sec(%ld) intercepted, using shim conversion\n", (long)sec);
	fflush(stdout);
	return sec * APR_USEC_PER_SEC;
}

apr_time_t apex_apr_time_usec(apr_time_t time)
{
	printf("[apex_apr] apr_time_usec(%ld) intercepted, using shim conversion\n", (long)time);
	fflush(stdout);
	return time % APR_USEC_PER_SEC;
}

#endif /* USE_APEX_API */

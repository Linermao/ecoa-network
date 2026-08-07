#include <apr_time.h>

#include <limits.h>

#ifdef USE_APEX_API
#undef apr_sleep
#undef apr_time_now
#undef apr_time_from_sec
#undef apr_time_usec

void apex_apr_sleep(apr_interval_time_t t)
{
	SYSTEM_TIME_TYPE delay;
	RETURN_CODE_TYPE return_code;

	if (t <= 0) {
		return;
	}

	/* APEX system time is expressed in nanoseconds. */
	if (t > INT64_MAX / 1000) {
		return;
	}
	delay = (SYSTEM_TIME_TYPE)(t * 1000);
	TIMED_WAIT(delay, &return_code);
}

apr_time_t apex_apr_time_now(void)
{
	SYSTEM_TIME_TYPE system_time;
	RETURN_CODE_TYPE return_code;

	GET_TIME(&system_time, &return_code);
	if (return_code != NO_ERROR || system_time < 0) {
		return 0;
	}
	return (apr_time_t)(system_time / 1000);
}

apr_time_t apex_apr_time_from_sec(apr_time_t sec)
{
	return sec * APR_USEC_PER_SEC;
}

apr_time_t apex_apr_time_usec(apr_time_t time)
{
	return time % APR_USEC_PER_SEC;
}

#endif /* USE_APEX_API */

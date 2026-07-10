#include <stdio.h>

#include <apr_errno.h>

#ifdef USE_APEX_API
#undef apr_strerror

/*
 * Error string formatting is isolated first because it gives the future APEX
 * status migration a single compatibility point for human-readable messages.
 */
char *apex_apr_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize)
{
	printf("[apex_apr] apr_strerror(%d, %p, %lu) intercepted, delegating to system APR\n",
	       statcode,
	       (void *)buf,
	       (unsigned long)bufsize);
	fflush(stdout);
	return apr_strerror(statcode, buf, bufsize);
}

#endif /* USE_APEX_API */

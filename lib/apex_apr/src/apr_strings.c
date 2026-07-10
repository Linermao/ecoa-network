#include <stdarg.h>
#include <stdio.h>

#include <apr_strings.h>

#ifdef USE_APEX_API
#undef apr_psprintf
#undef apr_cpystrn

/*
 * String helpers remain APR-backed for now because apr_psprintf depends on
 * pool allocation behavior. Intercepting here still gives the shim a stable
 * handoff point once the pool layer moves to APEX.
 */
char *apex_apr_psprintf(apr_pool_t *p, const char *fmt, ...)
{
	va_list ap;
	char *result;

	printf("[apex_apr] apr_psprintf(%p, %p, ...) intercepted, delegating to system APR\n",
	       (void *)p,
	       (const void *)fmt);
	fflush(stdout);

	va_start(ap, fmt);
	result = apr_pvsprintf(p, fmt, ap);
	va_end(ap);

	return result;
}

char *apex_apr_cpystrn(char *dst, const char *src, apr_size_t dst_size)
{
	printf("[apex_apr] apr_cpystrn(%p, %p, %lu) intercepted, delegating to system APR\n",
	       (void *)dst,
	       (const void *)src,
	       (unsigned long)dst_size);
	fflush(stdout);
	return apr_cpystrn(dst, src, dst_size);
}

#endif /* USE_APEX_API */

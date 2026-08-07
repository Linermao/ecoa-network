#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <apr_pools.h>
#include <apr_strings.h>

#ifdef USE_APEX_API
#undef apr_psprintf
#undef apr_cpystrn
#undef apr_pstrndup
#undef apr_pstrcat

/*
 * String helpers remain APR-backed for now because apr_psprintf depends on
 * pool allocation behavior. Intercepting here still gives the shim a stable
 * handoff point once the pool layer moves to APEX.
 */
char *apex_apr_psprintf(apr_pool_t *p, const char *fmt, ...)
{
	va_list ap;
	va_list copied_ap;
	int length;
	char *result;

	if (p == NULL || fmt == NULL) {
		return NULL;
	}

	va_start(ap, fmt);
	va_copy(copied_ap, ap);
	length = vsnprintf(NULL, 0, fmt, copied_ap);
	va_end(copied_ap);
	if (length < 0) {
		va_end(ap);
		return NULL;
	}

	result = apex_apr_palloc(p, (apr_size_t)length + 1);
	if (result != NULL) {
		(void)vsnprintf(result, (apr_size_t)length + 1, fmt, ap);
	}
	va_end(ap);

	return result;
}

char *apex_apr_cpystrn(char *dst, const char *src, apr_size_t dst_size)
{
	char *end;

	if (dst == NULL || src == NULL) {
		return dst;
	}

	end = dst;
	if (dst_size == 0) {
		return end;
	}

	while (dst_size > 1 && *src != '\0') {
		*end++ = *src++;
		--dst_size;
	}
	*end = '\0';
	return end;
}

char *apex_apr_pstrndup(apr_pool_t *p, const char *s, apr_size_t n)
{
	apr_size_t length;
	char *copy;

	if (p == NULL || s == NULL) {
		return NULL;
	}

	length = 0;
	while (length < n && s[length] != '\0') {
		++length;
	}

	copy = apex_apr_palloc(p, length + 1);
	if (copy == NULL) {
		return NULL;
	}
	if (length > 0) {
		memcpy(copy, s, length);
	}
	copy[length] = '\0';
	return copy;
}

char *apex_apr_pstrcat(apr_pool_t *p, ...)
{
	va_list ap;
	va_list copied_ap;
	const char *part;
	apr_size_t length;
	char *result;
	char *cursor;

	if (p == NULL) {
		return NULL;
	}

	length = 0;
	va_start(ap, p);
	va_copy(copied_ap, ap);
	while ((part = va_arg(copied_ap, const char *)) != NULL) {
		apr_size_t part_length;

		part_length = strlen(part);
		if (part_length > (apr_size_t)-1 - length - 1) {
			va_end(copied_ap);
			va_end(ap);
			return NULL;
		}
		length += part_length;
	}
	va_end(copied_ap);

	result = apex_apr_palloc(p, length + 1);
	if (result == NULL) {
		va_end(ap);
		return NULL;
	}

	cursor = result;
	while ((part = va_arg(ap, const char *)) != NULL) {
		apr_size_t part_length;

		part_length = strlen(part);
		memcpy(cursor, part, part_length);
		cursor += part_length;
	}
	va_end(ap);
	*cursor = '\0';
	return result;
}

#endif /* USE_APEX_API */

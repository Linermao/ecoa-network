#ifndef APEX_APR_APR_STRINGS_H
#define APEX_APR_APR_STRINGS_H

#include_next <apr_strings.h>

#ifdef USE_APEX_API

#ifdef __cplusplus
extern "C" {
#endif

/*
 * apr_strings.h depends on APR pool allocation semantics, so keep these
 * wrappers thin until the pool replacement story is settled. The goal here is
 * to lock down signatures and interception points without changing behavior.
 *
 * Future APEX migration plan:
 *   1. Route apr_psprintf through an APEX-aware pool formatter.
 *   2. Keep apr_cpystrn source-compatible unless the project deliberately
 *      adopts a different string utility contract.
 */
char *apex_apr_psprintf(apr_pool_t *p, const char *fmt, ...);
char *apex_apr_cpystrn(char *dst, const char *src, apr_size_t dst_size);

#ifdef __cplusplus
}
#endif

#define apr_psprintf apex_apr_psprintf
#define apr_cpystrn apex_apr_cpystrn

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_STRINGS_H */

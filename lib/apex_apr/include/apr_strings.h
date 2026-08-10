#ifndef APEX_APR_APR_STRINGS_H
#define APEX_APR_APR_STRINGS_H

#include_next <apr_strings.h>

#include "apex_apr_size.h"

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
char *apex_apr_pstrndup(apr_pool_t *p, const char *s, apr_size_t n);
char *apex_apr_pstrcat(apr_pool_t *p, ...);

#ifdef __cplusplus
}
#endif

#define apr_psprintf apex_apr_psprintf
#define apr_cpystrn apex_apr_cpystrn
#define apr_pstrndup apex_apr_pstrndup
#define apr_pstrcat apex_apr_pstrcat

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_STRINGS_H */

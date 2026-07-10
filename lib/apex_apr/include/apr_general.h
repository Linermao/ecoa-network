#ifndef APEX_APR_APR_GENERAL_H
#define APEX_APR_APR_GENERAL_H

#include_next <apr_general.h>

#ifdef USE_APEX_API

#ifdef __cplusplus
extern "C" {
#endif

/*
 * apr_general.h owns APR library lifetime hooks. Keep the wrapper signatures
 * source-compatible with APR so existing initialization order remains intact
 * while the implementation still delegates to system APR.
 *
 * Future APEX migration plan:
 *   1. Preserve the reference-counted initialize/terminate pairing.
 *   2. Replace the APR delegate call with APEX runtime setup/teardown.
 *   3. Keep apr_status_t/void return behavior source-compatible for callers.
 */
apr_status_t apex_apr_initialize(void);
void apex_apr_terminate(void);

#ifdef __cplusplus
}
#endif

#define apr_initialize apex_apr_initialize
#define apr_terminate apex_apr_terminate

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_GENERAL_H */

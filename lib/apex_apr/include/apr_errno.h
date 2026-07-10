#ifndef APEX_APR_APR_ERRNO_H
#define APEX_APR_APR_ERRNO_H

#include_next <apr_errno.h>

#ifdef USE_APEX_API

/*
 * Future APEX-backed status skeleton.
 *
 * This is intentionally separate from the public apr_status_t alias for now.
 * Existing APR code expects an integer status ABI, so the custom record shape
 * is documented here first and can be activated later with coordinated API
 * wrapper work.
 */
typedef struct apex_apr_status_layout {
	int code;
	int domain;
	void *detail;
} apex_apr_status_layout;

/*
 * Migrate apr_status_t at the apr_errno module boundary by bridging the
 * upstream typedef into a project-owned alias. This preserves the current ABI
 * and remains robust even if another APR header pulled in the system typedef
 * before this shim was reached.
 */
typedef apr_status_t apex_apr_status_t;
#define apr_status_t apex_apr_status_t

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keep apr_status_t ABI-compatible during the transition by mapping callers to
 * apex_apr_status_t, which currently aliases the upstream APR integer status
 * representation. Replace the type together with its status constants once the
 * APEX status model is defined, instead of trying to remap the typedef
 * piecemeal.
 *
 * Current compatibility boundary:
 *   1. apr_status_t is redirected to the project-owned apex_apr_status_t
 *      alias, while preserving the upstream APR integer ABI.
 *   2. APR_SUCCESS and APR_EGENERAL are pinned to the APR-compatible values
 *      used throughout this codebase.
 *   3. apr_strerror is intercepted independently so callers can keep using
 *      APR-style error formatting while the status model evolves.
 *
 * Future APEX migration plan:
 *   1. Define the project-wide APEX status domain.
 *   2. Replace apr_status_t, APR_SUCCESS, and APR_EGENERAL as one module.
 *   3. Teach apex_apr_strerror to format both legacy APR and APEX-native
 *      status codes during the transition.
 */
char *apex_apr_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize);

#ifdef __cplusplus
}
#endif

#ifdef APR_SUCCESS
#undef APR_SUCCESS
#endif
#define APR_SUCCESS 0

#ifdef APR_EGENERAL
#undef APR_EGENERAL
#endif
#define APR_EGENERAL (APR_OS_START_ERROR + 14)

#define apr_strerror apex_apr_strerror

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_ERRNO_H */

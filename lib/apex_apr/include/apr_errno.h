#ifndef APEX_APR_APR_ERRNO_H
#define APEX_APR_APR_ERRNO_H

#include_next <apr_errno.h>

#include <errno.h>
#include <apex_apr_size.h>

#ifdef USE_APEX_API

/*
 * Migrate apr_status_t at the apr_errno module boundary by bridging the
 * integer. This preserves the current ABI and remains robust even if another
 * APR header pulled in the system typedef before this shim was reached.
 */
typedef int apex_apr_status_t;
#define apr_status_t apex_apr_status_t

#ifdef __cplusplus
extern "C" {
#endif

/* APR-compatible ranges owned by the APEX APR status layer. */
#define APEX_APR_STATUS_ERROR_BASE 20000
#define APEX_APR_STATUS_STATUS_BASE 70000
#define APEX_APR_STATUS_CANONICAL_ERROR_BASE 620000

char *apex_apr_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize);
int apex_apr_status_is_success(apr_status_t status);
const char *apex_apr_status_to_string(apr_status_t status);

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
#define APR_EGENERAL (APEX_APR_STATUS_ERROR_BASE + 14)

#ifdef APR_CHILD_DONE
#undef APR_CHILD_DONE
#endif
#define APR_CHILD_DONE (APEX_APR_STATUS_STATUS_BASE + 5)

#ifdef APR_CHILD_NOTDONE
#undef APR_CHILD_NOTDONE
#endif
#define APR_CHILD_NOTDONE (APEX_APR_STATUS_STATUS_BASE + 6)

#ifdef APR_TIMEUP
#undef APR_TIMEUP
#endif
#define APR_TIMEUP (APEX_APR_STATUS_STATUS_BASE + 7)

#ifdef APR_EOF
#undef APR_EOF
#endif
#define APR_EOF (APEX_APR_STATUS_STATUS_BASE + 14)

#ifdef APR_ENOTIMPL
#undef APR_ENOTIMPL
#endif
#define APR_ENOTIMPL (APEX_APR_STATUS_STATUS_BASE + 23)

#ifdef APR_EBUSY
#undef APR_EBUSY
#endif
#define APR_EBUSY (APEX_APR_STATUS_STATUS_BASE + 25)

#ifdef APR_EEXIST
#undef APR_EEXIST
#endif
#ifdef EEXIST
#define APR_EEXIST EEXIST
#else
#define APR_EEXIST (APEX_APR_STATUS_CANONICAL_ERROR_BASE + 2)
#endif

#ifdef APR_ENOMEM
#undef APR_ENOMEM
#endif
#ifdef ENOMEM
#define APR_ENOMEM ENOMEM
#else
#define APR_ENOMEM (APEX_APR_STATUS_CANONICAL_ERROR_BASE + 7)
#endif

#ifdef APR_EINVAL
#undef APR_EINVAL
#endif
#ifdef EINVAL
#define APR_EINVAL EINVAL
#else
#define APR_EINVAL (APEX_APR_STATUS_CANONICAL_ERROR_BASE + 11)
#endif

#ifdef APR_EAGAIN
#undef APR_EAGAIN
#endif
#ifdef EAGAIN
#define APR_EAGAIN EAGAIN
#else
#define APR_EAGAIN (APEX_APR_STATUS_CANONICAL_ERROR_BASE + 13)
#endif

typedef enum apex_apr_status_code {
	APEX_APR_STATUS_SUCCESS = APR_SUCCESS,
	APEX_APR_STATUS_GENERAL = APR_EGENERAL,
	APEX_APR_STATUS_TIMEOUT = APR_TIMEUP,
	APEX_APR_STATUS_BUSY = APR_EBUSY,
	APEX_APR_STATUS_AGAIN = APR_EAGAIN,
	APEX_APR_STATUS_NO_MEMORY = APR_ENOMEM,
	APEX_APR_STATUS_INVALID = APR_EINVAL,
	APEX_APR_STATUS_EXISTS = APR_EEXIST,
	APEX_APR_STATUS_NOT_IMPLEMENTED = APR_ENOTIMPL,
	APEX_APR_STATUS_END_OF_FILE = APR_EOF,
	APEX_APR_STATUS_CHILD_DONE = APR_CHILD_DONE,
	APEX_APR_STATUS_CHILD_NOT_DONE = APR_CHILD_NOTDONE
} apex_apr_status_code;

#define apr_strerror apex_apr_strerror

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_ERRNO_H */

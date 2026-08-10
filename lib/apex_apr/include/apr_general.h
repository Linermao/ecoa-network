#ifndef APEX_APR_APR_GENERAL_H
#define APEX_APR_APR_GENERAL_H

#include_next <apr_general.h>

#include "apex_apr_size.h"

#ifdef USE_APEX_API

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These wrappers own the APEX APR lifecycle. Initialization is reference
 * counted so independently started runtime tasks can retain APR-compatible
 * pairing without starting or stopping the operating system itself.
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

#ifndef APEX_APR_APR_POLL_H
#define APEX_APR_APR_POLL_H

#include_next <apr_poll.h>

#ifdef USE_APEX_API

/*
 * Poll-related types are bridged as aliases first. This keeps the current TCP
 * and UDP socket model working while leaving room for a later APEX/event-port
 * style replacement.
 */
typedef apr_pollfd_t apex_apr_pollfd_t;
typedef apr_pollset_t apex_apr_pollset_t;

#define apr_pollfd_t apex_apr_pollfd_t
#define apr_pollset_t apex_apr_pollset_t

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_POLL_H */

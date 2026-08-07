#ifndef APEX_APR_APR_H
#define APEX_APR_APR_H

#include_next <apr.h>

#ifdef USE_APEX_API
#include <apex_apr_size.h>

/*
 * This project often treats apr.h as an umbrella header. Pull the shimmed
 * module headers in here so apr.h-only call sites still see the remaps.
 */
#include <apr_errno.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_time.h>
#endif

#endif /* APEX_APR_APR_H */

#ifndef APEX_APR_APR_H
#define APEX_APR_APR_H

#include_next <apr.h>

#ifdef USE_APEX_API
/*
 * Future scalar-wrapper skeletons.
 *
 * These are not wired in as the public APR scalar types yet. They exist to
 * document the eventual APEX-owned data shapes while the codebase still relies
 * on APR's integer and size_t ABI.
 */
typedef struct apex_apr_size_layout {
	size_t value;
	void *allocator_state;
} apex_apr_size_layout;

/*
 * Scalar APR types are bridged here because apr.h is their owning header.
 * Keep the underlying ABI identical to APR for now while moving the public
 * type names under project ownership.
 */
typedef apr_size_t apex_apr_size_t;
typedef apr_int32_t apex_apr_int32_t;
typedef apr_int64_t apex_apr_int64_t;

#define apr_size_t apex_apr_size_t
#define apr_int32_t apex_apr_int32_t
#define apr_int64_t apex_apr_int64_t

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

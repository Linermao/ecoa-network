#ifndef APEX_APR_SCALAR_TYPES_H
#define APEX_APR_SCALAR_TYPES_H

#include <stddef.h>
#include <limits.h>
#include <stdint.h>

#ifdef USE_APEX_API

/*
 * These are scalar ABI types, not runtime APEX objects. Keep their standard C
 * representations so buffer sizes, fixed-width values, and APR call
 * signatures retain their required semantics on every target.
 */
typedef size_t apex_apr_size_t;
typedef int32_t apex_apr_int32_t;
typedef int64_t apex_apr_int64_t;

#ifndef apr_size_t
#define apr_size_t apex_apr_size_t
#endif

#ifndef apr_int32_t
#define apr_int32_t apex_apr_int32_t
#endif

#ifndef apr_int64_t
#define apr_int64_t apex_apr_int64_t
#endif

typedef char apex_apr_size_t_must_match_size_t[
	(sizeof(apex_apr_size_t) == sizeof(size_t)) ? 1 : -1];
typedef char apex_apr_int32_t_must_be_32_bits[
	(sizeof(apex_apr_int32_t) * CHAR_BIT == 32) ? 1 : -1];
typedef char apex_apr_int64_t_must_be_64_bits[
	(sizeof(apex_apr_int64_t) * CHAR_BIT == 64) ? 1 : -1];

#endif /* USE_APEX_API */

#endif /* APEX_APR_SCALAR_TYPES_H */

#ifndef APEX_APR_APR_TIME_H
#define APEX_APR_APR_TIME_H

#include_next <apr_time.h>

#ifdef USE_APEX_API
#include <stdint.h>

typedef int64_t apex_apr_time_t;
typedef int64_t apex_apr_interval_time_t;
typedef int32_t apex_apr_short_interval_time_t;
#define apr_time_t apex_apr_time_t
#define apr_interval_time_t apex_apr_interval_time_t
#define apr_short_interval_time_t apex_apr_short_interval_time_t

#include <a653Time.h>


#ifdef __cplusplus
extern "C" {
#endif

void apex_apr_sleep(apr_interval_time_t t);
apr_time_t apex_apr_time_now(void);
apr_time_t apex_apr_time_from_sec(apr_time_t sec);
apr_time_t apex_apr_time_usec(apr_time_t time);


#ifdef __cplusplus
}
#endif

#ifdef apr_time_from_sec
#undef apr_time_from_sec
#endif

#ifdef apr_time_usec
#undef apr_time_usec
#endif

#define apr_sleep apex_apr_sleep
#define apr_time_now apex_apr_time_now
#define apr_time_from_sec apex_apr_time_from_sec
#define apr_time_usec apex_apr_time_usec


#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_TIME_H */

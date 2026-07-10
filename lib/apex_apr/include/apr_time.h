#ifndef APEX_APR_APR_TIME_H
#define APEX_APR_APR_TIME_H

#include_next <apr_time.h>

#ifdef USE_APEX_API

/*
 * Future APEX-backed time skeleton.
 *
 * The value keeps microsecond semantics because that already matches the
 * project's current APR usage.
 */
typedef struct apex_apr_time_layout {
	apr_int64_t usec_value;
	void *clock_state;
} apex_apr_time_layout;

/*
 * Time types keep APR's integer ABI for now. This preserves the current
 * microsecond semantics while making the public names project-owned.
 */
typedef apr_time_t apex_apr_time_t;
#define apr_time_t apex_apr_time_t

#ifdef __cplusplus
extern "C" {
#endif

void apex_apr_sleep(apr_interval_time_t t);
apr_time_t apex_apr_time_now(void);
apr_time_t apex_apr_time_from_sec(apr_time_t sec);
apr_time_t apex_apr_time_usec(apr_time_t time);

/*
 * Add future apr_time.h replacements here with the same pattern:
 *
 *   1. Keep #include_next <apr_time.h> above so the original APR types,
 *      constants, and declarations remain visible during the transition.
 *   2. Declare a wrapper named apex_apr_<original_name> in this extern "C"
 *      block. Keep the signature source-compatible with APR unless there is
 *      an intentional project-wide API change.
 *   3. Add the matching macro remap below the extern "C" block.
 *   4. Implement the wrapper in ../src/apr_time.c. In the implementation file,
 *      #undef the APR macro before defining the wrapper; otherwise calls to
 *      the original APR function will recursively expand back to the wrapper.
 *   5. The current migration style is "printf + delegate to system APR" for
 *      runtime-dependent APIs and "printf + local compatibility math" for APR
 *      macros such as apr_time_from_sec/apr_time_usec.
 *   6. Replace the delegate call with APEX API calls only when that behavior
 *      is ready and tested.
 */

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

/*
 * Add future macro remaps here, one per wrapper declaration above, for example:
 *
 *   #define apr_time_now apex_apr_time_now
 *
 * Do not remap APR types here. Type compatibility should be handled by the
 * corresponding shim header and implementation strategy for that APR module.
 */

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_TIME_H */

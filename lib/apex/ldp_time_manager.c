/**
 * @file ldp_time_manager.c
 * @brief APEX implementation of the LDP time-manager boundary.
 */

#include <apr_time.h>

#include "ldp_time_manager.h"

static void apex_ldp_time_now(ldp__timestamp *time)
{
	apr_time_t now = apr_time_now();

	if (time == NULL) {
		return;
	}
	time->seconds = (uint32_t)(now / APR_USEC_PER_SEC);
	time->nanoseconds = (uint32_t)((now % APR_USEC_PER_SEC) * 1000);
}

void ldp_get_time(ldp__timestamp *time)
{
	apex_ldp_time_now(time);
}

void ldp_get_ecoa_utc_time(ldp__timestamp *time)
{
	apex_ldp_time_now(time);
}

void ldp_get_ecoa_absolute_time(ldp__timestamp *time)
{
	apex_ldp_time_now(time);
}

void ldp_get_ecoa_relative_time(const ldp__timestamp *reference,
	ldp__timestamp *time)
{
	apex_ldp_time_now(time);
	if (reference != NULL && time != NULL) {
		ldp_subs_time(time, reference);
	}
}

static void apex_ldp_time_resolution(ldp__timestamp *resolution)
{
	if (resolution != NULL) {
		resolution->seconds = 0;
		resolution->nanoseconds = 1000;
	}
}

void ldp_get_ecoa_utc_timeres(ldp__timestamp *resolution)
{
	apex_ldp_time_resolution(resolution);
}

void ldp_get_ecoa_absolute_timeres(ldp__timestamp *resolution)
{
	apex_ldp_time_resolution(resolution);
}

void ldp_get_ecoa_relative_timeres(ldp__timestamp *resolution)
{
	apex_ldp_time_resolution(resolution);
}

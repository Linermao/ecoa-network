/**
 * @file ldp_thread.c
 * @brief APEX implementation boundary for ECOA thread creation.
 *
 * APEX/ARINC 653 schedules PROCESSes rather than POSIX threads.  Do not move
 * the Unix policy, niceness, affinity, or pthread-name code into this file:
 * those concepts have no portable APEX equivalent.  The APR shim owns the
 * eventual mapping to CREATE_PROCESS and START. APEX process priority and
 * timing parameters have target configuration constraints, so this boundary
 * deliberately leaves those defaults in the APR/APEX shim for now.
 */

#include "ldp_thread.h"

ldp_status_t ldp_thread_create(apr_thread_t **new_thread,
                               apr_threadattr_t *attr,
                               apr_thread_start_t func,
                               void *data,
                               ldp_thread_properties *properties,
                               apr_pool_t *pool)
{
	/*
	 * The deployment descriptor will later map process name, priority, stack
	 * size, period, time capacity, and deadline into PROCESS_ATTRIBUTE_TYPE.
	 * Passing it through unchanged today keeps the public LDP call contract
	 * while avoiding accidental use of POSIX-only fields on an APEX target.
	 */
	if (properties != NULL) {
		apex_apr_threadattr_set_name(attr, properties->thread_name);
	}

	return apr_thread_create(new_thread, attr, func, data, pool);
}

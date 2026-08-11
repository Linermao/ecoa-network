/**
 * @file ldp_trigger.c
 * @brief APEX Process implementation for periodic ECOA triggers.
 *
 * The Unix implementation uses POSIX timers, signals and pthread barriers.
 * APEX has none of those services, so each configured periodic event is an
 * APEX-backed APR thread/process which waits for its period and dispatches
 * the configured operations.
 */

#include <assert.h>

#include <apr.h>
#include <apr_thread_proc.h>

#include "ldp_trigger.h"
#include "ldp_mod_container_util.h"

static apr_interval_time_t apex_ldp_trigger_period(float period)
{
	if (period <= 0.0f) {
		return 1;
	}
	return (apr_interval_time_t)(period * APR_USEC_PER_SEC);
}

static void *apex_ldp_trigger_event(apr_thread_t *thread, void *data)
{
	ldp_event_thread_timer_attr *event_attr = data;
	ldp_trigger_context *ctx = event_attr->ctx;
	ldp_trigger_event_context *event =
		&ctx->trigger_events[event_attr->trigger_event_index];
	apr_interval_time_t period = apex_ldp_trigger_period(event->period);
	char message[LDP_HEADER_TCP_SIZE] = {0};
	int i;

	(void)thread;
	while (ctx->state == RUNNING) {
		apr_sleep(period);
		if (ctx->state != RUNNING) {
			break;
		}
		for (i = 0; i < event->nb_operations; ++i) {
			int operation_index = event->operation_indexes[i];

			ldp_mod_event_send_local((ldp_module_context *)ctx, message, 0,
				ctx->operation_map[operation_index], false);
		}
	}
	return NULL;
}

static apr_status_t apex_ldp_trigger_start_events(
	ldp_trigger_context *ctx, apr_thread_t **event_threads,
	ldp_event_thread_timer_attr *event_attrs)
{
	apr_threadattr_t *attr;
	apr_status_t status;
	int i;

	for (i = 0; i < ctx->nb_trigger_event; ++i) {
		status = apr_threadattr_create(&attr, ctx->mem_pool);
		if (status != APR_SUCCESS) {
			return status;
		}
		event_attrs[i].ctx = ctx;
		event_attrs[i].trigger_event_index = i;
		status = apr_thread_create(&event_threads[i], attr,
			apex_ldp_trigger_event, &event_attrs[i], ctx->mem_pool);
		if (status != APR_SUCCESS) {
			return status;
		}
	}
	return APR_SUCCESS;
}

static void apex_ldp_trigger_stop_events(ldp_trigger_context *ctx,
	apr_thread_t **event_threads)
{
	apr_status_t result;
	int i;

	ctx->state = READY;
	for (i = 0; i < ctx->nb_trigger_event; ++i) {
		if (event_threads[i] != NULL) {
			(void)apr_thread_join(&result, event_threads[i]);
			event_threads[i] = NULL;
		}
	}
}

void *ldp_start_module_trigger(apr_thread_t *thread, void *data)
{
	ldp_trigger_context *ctx = data;
	ldp_element *element;
	apr_thread_t **event_threads;
	ldp_event_thread_timer_attr *event_attrs;
	int running = 1;

	ctx->mem_pool = apr_thread_pool_get(thread);
	event_threads = apr_pcalloc(ctx->mem_pool,
		sizeof(*event_threads) * (apr_size_t)ctx->nb_trigger_event);
	event_attrs = apr_pcalloc(ctx->mem_pool,
		sizeof(*event_attrs) * (apr_size_t)ctx->nb_trigger_event);
	if (event_threads == NULL || event_attrs == NULL) {
		return NULL;
	}
	ctx->state = IDLE;

	while (running) {
		ldp_fifo_manager_pop_elt(ctx->fifo_manager, &element);
		switch (element->op_ID) {
		case LDP_ID_INITIALIZE_life:
			if (ctx->state == IDLE) {
				ctx->state = READY;
				ldp_mod_init_notify((ldp_module_context *)ctx);
			}
			break;
		case LDP_ID_START_life:
			if (ctx->state == READY) {
				ctx->state = RUNNING;
				if (apex_ldp_trigger_start_events(ctx, event_threads,
					event_attrs) != APR_SUCCESS) {
					apex_ldp_trigger_stop_events(ctx, event_threads);
				}
			}
			break;
		case LDP_ID_STOP_life:
			if (ctx->state == RUNNING) {
				apex_ldp_trigger_stop_events(ctx, event_threads);
				ldp_fifo_manager_clean(ctx->fifo_manager, ctx->state, NULL);
			}
			break;
		case LDP_ID_SHUTDOWN_life:
			if (ctx->state == RUNNING) {
				apex_ldp_trigger_stop_events(ctx, event_threads);
			}
			ctx->state = IDLE;
			ldp_fifo_manager_clean(ctx->fifo_manager, ctx->state, NULL);
			break;
		case LDP_ID_KILL_life:
			if (ctx->state == RUNNING) {
				apex_ldp_trigger_stop_events(ctx, event_threads);
			}
			ctx->state = IDLE;
			running = 0;
			break;
		default:
			break;
		}
		(void)ldp_fifo_manager_release_elt(ctx->fifo_manager, element);
	}
	return NULL;
}

/*
 * APR thread/process shim.
 *
 * APR threads are represented by APEX/ARINC 653 PROCESSes. The standard APEX
 * entry point has no parameters, so a fixed registry connects GET_MY_ID() to
 * the APR callback and its data. The target integration point is intentionally
 * limited to this file and apr_thread_proc.h.
 */

#include <stdio.h>
#include <string.h>

#include <apr_thread_proc.h>

#ifdef USE_APEX_API
#undef apr_threadattr_create
#undef apr_thread_create
#undef apr_thread_join
#undef apr_thread_pool_get

#define APEX_APR_THREAD_MAX_PROCESSES 64u
#define APEX_APR_THREAD_DEFAULT_STACK_SIZE 16384u
#define APEX_APR_THREAD_DEFAULT_PRIORITY 1

static apr_thread_t *apex_apr_threads[APEX_APR_THREAD_MAX_PROCESSES];
static unsigned int apex_apr_thread_sequence;

static apr_status_t apex_apr_thread_status_to_apr(RETURN_CODE_TYPE return_code)
{
	if (return_code == NO_ERROR) {
		return APR_SUCCESS;
	}
	if (return_code == NOT_AVAILABLE || return_code == NO_ACTION) {
		return APR_EBUSY;
	}
	if (return_code == INVALID_PARAM || return_code == INVALID_CONFIG) {
		return APR_EINVAL;
	}
	return APR_EGENERAL;
}

static apr_thread_t *apex_apr_thread_find(PROCESS_ID_TYPE process_id)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_THREAD_MAX_PROCESSES; ++i) {
		if (apex_apr_threads[i] != NULL &&
		    apex_apr_threads[i]->process_id == process_id) {
			return apex_apr_threads[i];
		}
	}
	return NULL;
}

static int apex_apr_thread_register(apr_thread_t *thread)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_THREAD_MAX_PROCESSES; ++i) {
		if (apex_apr_threads[i] == NULL) {
			apex_apr_threads[i] = thread;
			return 1;
		}
	}
	return 0;
}

static void apex_apr_thread_unregister(apr_thread_t *thread)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_THREAD_MAX_PROCESSES; ++i) {
		if (apex_apr_threads[i] == thread) {
			apex_apr_threads[i] = NULL;
			return;
		}
	}
}

static void apex_apr_thread_entry(void)
{
	PROCESS_ID_TYPE process_id;
	RETURN_CODE_TYPE return_code;
	apr_thread_t *thread;

	GET_MY_ID(&process_id, &return_code);
	if (return_code != NO_ERROR) {
		STOP_SELF();
		return;
	}
	thread = apex_apr_thread_find(process_id);
	if (thread != NULL && thread->start_routine != NULL) {
		(void)thread->start_routine(thread, thread->baton);
		thread->result = APR_SUCCESS;
		thread->completed = 1;
		SIGNAL_SEMAPHORE(thread->completion_semaphore_id, &return_code);
	}
	STOP_SELF();
}

apr_status_t apex_apr_threadattr_create(apr_threadattr_t **new_attr,
                                        apr_pool_t *pool)
{
	apr_threadattr_t *created;

	if (new_attr == NULL || pool == NULL) {
		return APR_EINVAL;
	}
	created = apr_pcalloc(pool, sizeof(*created));
	if (created == NULL) {
		return APR_ENOMEM;
	}
	created->stack_size = APEX_APR_THREAD_DEFAULT_STACK_SIZE;
	created->base_priority = APEX_APR_THREAD_DEFAULT_PRIORITY;
	created->period = INFINITE_TIME_VALUE;
	created->time_capacity = INFINITE_TIME_VALUE;
	created->deadline = HARD;
	*new_attr = created;
	return APR_SUCCESS;
}

apr_status_t apex_apr_thread_create(apr_thread_t **new_thread,
                                    apr_threadattr_t *attr,
                                    apr_thread_start_t func,
                                    void *data,
                                    apr_pool_t *pool)
{
	apr_thread_t *created;
	PROCESS_ATTRIBUTE_TYPE attributes;
	SEMAPHORE_NAME_TYPE semaphore_name;
	PROCESS_NAME_TYPE process_name;
	RETURN_CODE_TYPE return_code;
	apr_status_t status;

	if (new_thread == NULL || func == NULL || pool == NULL) {
		return APR_EINVAL;
	}
	if (attr == NULL) {
		status = apex_apr_threadattr_create(&attr, pool);
		if (status != APR_SUCCESS) {
			return status;
		}
	}
	created = apr_pcalloc(pool, sizeof(*created));
	if (created == NULL) {
		return APR_ENOMEM;
	}
	if (!apex_apr_thread_register(created)) {
		return APR_ENOMEM;
	}
	(void)snprintf(semaphore_name, sizeof(semaphore_name), "APRJD%u",
		++apex_apr_thread_sequence);
	CREATE_SEMAPHORE(semaphore_name, 0, 1, FIFO,
		&created->completion_semaphore_id, &return_code);
	status = apex_apr_thread_status_to_apr(return_code);
	if (status != APR_SUCCESS) {
		apex_apr_thread_unregister(created);
		return status;
	}

	if (attr->name[0] != '\0') {
		memcpy(process_name, attr->name, sizeof(process_name));
	} else {
		(void)snprintf(process_name, sizeof(process_name), "APRPR%u",
			apex_apr_thread_sequence);
	}
	memset(&attributes, 0, sizeof(attributes));
	attributes.PERIOD = attr->period;
	attributes.TIME_CAPACITY = attr->time_capacity;
	attributes.ENTRY_POINT = apex_apr_thread_entry;
	attributes.STACK_SIZE = attr->stack_size;
	attributes.BASE_PRIORITY = attr->base_priority;
	attributes.DEADLINE = attr->deadline;
	memcpy(attributes.NAME, process_name, sizeof(attributes.NAME));
	created->owner_pool = pool;
	created->start_routine = func;
	created->baton = data;
	CREATE_PROCESS(&attributes, &created->process_id, &return_code);
	status = apex_apr_thread_status_to_apr(return_code);
	if (status != APR_SUCCESS) {
		apex_apr_thread_unregister(created);
		return status;
	}
	START(created->process_id, &return_code);
	status = apex_apr_thread_status_to_apr(return_code);
	if (status != APR_SUCCESS) {
		apex_apr_thread_unregister(created);
		return status;
	}
	created->active = 1;
	*new_thread = created;
	return APR_SUCCESS;
}

void apex_apr_threadattr_set_name(apr_threadattr_t *attr, const char *name)
{
	if (attr == NULL) {
		return;
	}
	memset(attr->name, 0, sizeof(attr->name));
	if (name != NULL) {
		(void)snprintf(attr->name, sizeof(attr->name), "%s", name);
	}
}

apr_status_t apex_apr_thread_join(apr_status_t *retval,
                                  apr_thread_t *thread)
{
	RETURN_CODE_TYPE return_code;
	apr_status_t status;

	if (retval == NULL || thread == NULL || !thread->active) {
		return APR_EINVAL;
	}
	WAIT_SEMAPHORE(thread->completion_semaphore_id, INFINITE_TIME_VALUE,
		&return_code);
	status = apex_apr_thread_status_to_apr(return_code);
	if (status != APR_SUCCESS) {
		return status;
	}
	*retval = thread->result;
	thread->active = 0;
	apex_apr_thread_unregister(thread);
	return APR_SUCCESS;
}

apr_pool_t *apex_apr_thread_pool_get(const apr_thread_t *thread)
{
	return thread != NULL && thread->active ? thread->owner_pool : NULL;
}

#endif /* USE_APEX_API */

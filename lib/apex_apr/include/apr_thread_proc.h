#ifndef APEX_APR_APR_THREAD_PROC_H
#define APEX_APR_APR_THREAD_PROC_H

#ifdef USE_APEX_API
/*
 * Establish the shimmed pool type before APR declares thread APIs which take
 * an apr_pool_t argument. This keeps the declarations consistent regardless
 * of the order in which project headers include pools and thread/process APIs.
 */
#include <apr_pools.h>
#include <a653Process.h>
#include <a653Semaphore.h>
#endif

#include_next <apr_thread_proc.h>

#ifdef USE_APEX_API

typedef struct apex_apr_thread_layout {
	PROCESS_ID_TYPE process_id;
	SEMAPHORE_ID_TYPE completion_semaphore_id;
	apr_pool_t *owner_pool;
	void *(*start_routine)(struct apex_apr_thread_layout *thread, void *data);
	void *baton;
	apr_status_t result;
	int active;
	int completed;
} apex_apr_thread_layout;

typedef struct apex_apr_threadattr_layout {
	STACK_SIZE_TYPE stack_size;
	PRIORITY_TYPE base_priority;
	SYSTEM_TIME_TYPE period;
	SYSTEM_TIME_TYPE time_capacity;
	DEADLINE_TYPE deadline;
	PROCESS_NAME_TYPE name;
} apex_apr_threadattr_layout;

typedef struct apex_apr_proc_layout {
	void *native_proc;
	void *runtime_descriptor;
	void *io_state;
	void *exit_state;
} apex_apr_proc_layout;

typedef struct apex_apr_procattr_layout {
	void *launch_config;
	void *io_config;
	void *runtime_policy;
} apex_apr_procattr_layout;

typedef apex_apr_thread_layout apex_apr_thread_t;
typedef apex_apr_threadattr_layout apex_apr_threadattr_t;
typedef apr_proc_t apex_apr_proc_t;
typedef apr_procattr_t apex_apr_procattr_t;
typedef apr_exit_why_e apex_apr_exit_why_e;

typedef void *(*apex_apr_thread_start_t)(apex_apr_thread_t *thread,
							 void *data);

#define apr_thread_t apex_apr_thread_t
#define apr_threadattr_t apex_apr_threadattr_t
#define apr_thread_start_t apex_apr_thread_start_t
#define apr_proc_t apex_apr_proc_t
#define apr_procattr_t apex_apr_procattr_t
#define apr_exit_why_e apex_apr_exit_why_e

/*
 * Keep the first thread shim intentionally narrow. These are the process
 * thread APIs currently used by LDP. They deliberately do not delegate to
 * system APR: an APEX pool is not binary-compatible with an APR pool, and a
 * POSIX thread is not an ARINC 653 PROCESS. The source maps creation to the
 * standard APEX Process Service and isolates the target-facing types here.
 *
 * The eventual mapping is CREATE_PROCESS + START. Its entry-point trampoline
 * will recover the APR callback/context from the APEX process identifier.
 * JOIN needs a separate completion semaphore/event because APEX has no direct
 * PROCESS join primitive.
 */
apr_status_t apex_apr_threadattr_create(apr_threadattr_t **new_attr,
                                        apr_pool_t *pool);
apr_status_t apex_apr_thread_create(apr_thread_t **new_thread,
                                    apr_threadattr_t *attr,
                                    apr_thread_start_t func,
                                    void *data,
                                    apr_pool_t *pool);
apr_status_t apex_apr_thread_join(apr_status_t *retval,
                                  apr_thread_t *thread);
apr_pool_t *apex_apr_thread_pool_get(const apr_thread_t *thread);
void apex_apr_threadattr_set_name(apr_threadattr_t *attr, const char *name);

#define apr_threadattr_create apex_apr_threadattr_create
#define apr_thread_create apex_apr_thread_create
#define apr_thread_join apex_apr_thread_join
#define apr_thread_pool_get apex_apr_thread_pool_get

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_PROC_H */

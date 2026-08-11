#ifndef APEX_APR_APR_THREAD_PROC_H
#define APEX_APR_APR_THREAD_PROC_H

#ifdef USE_APEX_API
/*
 * Establish the shimmed pool type before APR declares thread APIs which take
 * an apr_pool_t argument. This keeps the declarations consistent regardless
 * of the order in which project headers include pools and thread/process APIs.
 */
#include <apr_pools.h>
#endif

#include_next <apr_thread_proc.h>

#ifdef USE_APEX_API

struct apex_apr_pool_layout;

/*
 * Thread and process types are bridged here as project-owned aliases first.
 * The current goal is to secure the public type names while deferring the
 * eventual APEX-specific handle layouts and static configuration details.
 */
typedef struct apex_apr_thread_layout {
	void *native_thread;
	struct apex_apr_pool_layout *owner_pool;
	void *attr_state;
	void *start_state;
	void *baton;
} apex_apr_thread_layout;

typedef struct apex_apr_threadattr_layout {
	void *stack_state;
	void *sched_state;
	void *runtime_policy;
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

typedef apr_thread_t apex_apr_thread_t;
typedef apr_threadattr_t apex_apr_threadattr_t;
typedef apr_proc_t apex_apr_proc_t;
typedef apr_procattr_t apex_apr_procattr_t;
typedef apr_exit_why_e apex_apr_exit_why_e;

#define apr_thread_t apex_apr_thread_t
#define apr_threadattr_t apex_apr_threadattr_t
#define apr_proc_t apex_apr_proc_t
#define apr_procattr_t apex_apr_procattr_t
#define apr_exit_why_e apex_apr_exit_why_e

/*
 * Keep the first thread shim intentionally narrow. These are the process
 * thread APIs currently used by LDP. They deliberately do not delegate to
 * system APR: an APEX pool is not binary-compatible with an APR pool, and a
 * POSIX thread is not an ARINC 653 PROCESS. The source currently fails these
 * calls explicitly until the target APEX Process Service is bound.
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

#define apr_threadattr_create apex_apr_threadattr_create
#define apr_thread_create apex_apr_thread_create
#define apr_thread_join apex_apr_thread_join
#define apr_thread_pool_get apex_apr_thread_pool_get

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_THREAD_PROC_H */

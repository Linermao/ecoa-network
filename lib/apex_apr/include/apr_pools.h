#ifndef APEX_APR_APR_POOLS_H
#define APEX_APR_APR_POOLS_H

#include_next <apr_pools.h>

#include <apex_apr_size.h>

#ifdef USE_APEX_API

#include <a653Type.h>
#include <a653Mutex.h>

#ifndef INFINITE_TIME_VALUE
#define INFINITE_TIME_VALUE ((SYSTEM_TIME_TYPE)-1)
#endif

/*
 * APEX-backed pool implementation. Its fields are declared here so related
 * APEX modules share one stable representation.
 *
 * apr_pool_t is deliberately redirected to this type. The pools module no
 * longer creates or depends on a system APR companion pool.
 */
typedef struct apex_apr_pool_t apex_apr_pool_t;

/*
 * The original APR declarations remain visible through include_next, while
 * code including this shim uses the APEX-owned pool representation below.
 */
#define apr_pool_t apex_apr_pool_t

typedef struct apex_apr_pool_block {
	struct apex_apr_pool_block *next;
	apr_size_t capacity;
	apr_size_t used;
	int from_host_heap;
} apex_apr_pool_block;

typedef struct apex_apr_pool_cleanup {
	struct apex_apr_pool_cleanup *next;
	const void *data;
	apr_status_t (*plain_cleanup)(void *);
	apr_status_t (*child_cleanup)(void *);
	int in_use;
} apex_apr_pool_cleanup;

typedef struct apex_apr_pool_userdata {
	struct apex_apr_pool_userdata *next;
	const void *data;
	const char *key;
	int in_use;
} apex_apr_pool_userdata;

struct apex_apr_pool_t {
	apex_apr_pool_t *parent;
	apex_apr_pool_t *first_child;
	apex_apr_pool_t *next_sibling;
	apex_apr_pool_block *blocks;
	apex_apr_pool_cleanup *cleanups;
	apex_apr_pool_cleanup *pre_cleanups;
	apex_apr_pool_userdata *userdata;
	apr_abortfunc_t abort_fn;
	const char *tag;
	int active;
	int unmanaged;
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The wrappers below implement the pool lifecycle and allocation APIs over
 * the APEX-owned static arena. APEX builds bind the arena to caller-owned
 * memory because a653lib does not expose a Memory Block service here.
 * They do not delegate to system APR pools.
 */
apr_status_t apex_apr_pool_create(apr_pool_t **newpool, apr_pool_t *parent);
apr_status_t apex_apr_pool_create_ex(apr_pool_t **newpool, apr_pool_t *parent,
						 apr_abortfunc_t abort_fn, apr_allocator_t *allocator);
apr_status_t apex_apr_pool_create_core_ex(apr_pool_t **newpool,
						      apr_abortfunc_t abort_fn, apr_allocator_t *allocator);
apr_status_t apex_apr_pool_create_unmanaged_ex(apr_pool_t **newpool,
						       apr_abortfunc_t abort_fn, apr_allocator_t *allocator);
apr_status_t apex_apr_pool_initialize(void);
void apex_apr_pool_terminate(void);
/* The static arena has no separately exposed apr_allocator_t object. */
apr_allocator_t *apex_apr_pool_allocator_get(apr_pool_t *pool);
void apex_apr_pool_clear(apr_pool_t *p);
void apex_apr_pool_destroy(apr_pool_t *p);
void *apex_apr_palloc(apr_pool_t *p, apr_size_t size);
void *apex_apr_pcalloc(apr_pool_t *p, apr_size_t size);

void apex_apr_pool_abort_set(apr_abortfunc_t abortfunc, apr_pool_t *pool);
apr_abortfunc_t apex_apr_pool_abort_get(apr_pool_t *pool);
apr_pool_t *apex_apr_pool_parent_get(apr_pool_t *pool);
int apex_apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b);
void apex_apr_pool_tag(apr_pool_t *pool, const char *tag);
apr_size_t apex_apr_pool_num_bytes(apr_pool_t *pool, int recurse);

apr_status_t apex_apr_pool_userdata_set(const void *data, const char *key,
						apr_status_t (*cleanup)(void *), apr_pool_t *pool);
apr_status_t apex_apr_pool_userdata_setn(const void *data, const char *key,
						 apr_status_t (*cleanup)(void *), apr_pool_t *pool);
apr_status_t apex_apr_pool_userdata_get(void **data, const char *key,
						apr_pool_t *pool);

void apex_apr_pool_cleanup_register(apr_pool_t *p, const void *data,
						 apr_status_t (*plain_cleanup)(void *),
						 apr_status_t (*child_cleanup)(void *));
void apex_apr_pool_pre_cleanup_register(apr_pool_t *p, const void *data,
						     apr_status_t (*plain_cleanup)(void *));
void apex_apr_pool_cleanup_kill(apr_pool_t *p, const void *data,
						 apr_status_t (*cleanup)(void *));
void apex_apr_pool_child_cleanup_set(apr_pool_t *p, const void *data,
						  apr_status_t (*plain_cleanup)(void *),
						  apr_status_t (*child_cleanup)(void *));
apr_status_t apex_apr_pool_cleanup_run(apr_pool_t *p, void *data,
						   apr_status_t (*cleanup)(void *));
apr_status_t apex_apr_pool_cleanup_null(void *data);
void apex_apr_pool_cleanup_for_exec(void);

/*
 * Bind the private allocator to a caller-owned region.
 */
apr_status_t apex_apr_pool_arena_configure(void *memory, apr_size_t size);
/*
 * a653lib does not currently expose a Memory Block service. This entry point
 * is kept for source compatibility and returns APR_ENOTIMPL.
 */
apr_status_t apex_apr_pool_arena_configure_memory_block(
	MEMORY_BLOCK_NAME_TYPE memory_block_name);
/*
 * Host-only compatibility switch for the C runtime heap. APEX targets reject
 * this mode.
 */
apr_status_t apex_apr_pool_use_host_heap(int enabled);
int apex_apr_pool_uses_private_allocator(const apr_pool_t *pool);

/* Internal accessors for future APEX-backed APR module wrappers. */
apex_apr_pool_t *apex_apr_pool_get(const apr_pool_t *pool);
apr_pool_t *apex_apr_pool_find(const void *mem);
void apex_apr_pool_join(apr_pool_t *pool, apr_pool_t *sub);

#ifdef __cplusplus
}
#endif

#ifdef apr_pool_create
#undef apr_pool_create
#endif
#define apr_pool_create apex_apr_pool_create

#ifdef apr_pool_create_ex
#undef apr_pool_create_ex
#endif
#define apr_pool_create_ex apex_apr_pool_create_ex

#ifdef apr_pool_create_core_ex
#undef apr_pool_create_core_ex
#endif
#define apr_pool_create_core_ex apex_apr_pool_create_core_ex

#ifdef apr_pool_create_unmanaged_ex
#undef apr_pool_create_unmanaged_ex
#endif
#define apr_pool_create_unmanaged_ex apex_apr_pool_create_unmanaged_ex

#ifdef apr_pool_initialize
#undef apr_pool_initialize
#endif
#define apr_pool_initialize apex_apr_pool_initialize

#ifdef apr_pool_terminate
#undef apr_pool_terminate
#endif
#define apr_pool_terminate apex_apr_pool_terminate

#ifdef apr_pool_allocator_get
#undef apr_pool_allocator_get
#endif
#define apr_pool_allocator_get apex_apr_pool_allocator_get

#ifdef apr_pool_clear
#undef apr_pool_clear
#endif
#define apr_pool_clear apex_apr_pool_clear

#ifdef apr_pool_destroy
#undef apr_pool_destroy
#endif
#define apr_pool_destroy apex_apr_pool_destroy

#ifdef apr_palloc
#undef apr_palloc
#endif
#define apr_palloc apex_apr_palloc

#ifdef apr_pcalloc
#undef apr_pcalloc
#endif
#define apr_pcalloc apex_apr_pcalloc

#ifdef apr_pool_abort_set
#undef apr_pool_abort_set
#endif
#define apr_pool_abort_set apex_apr_pool_abort_set

#ifdef apr_pool_abort_get
#undef apr_pool_abort_get
#endif
#define apr_pool_abort_get apex_apr_pool_abort_get

#ifdef apr_pool_parent_get
#undef apr_pool_parent_get
#endif
#define apr_pool_parent_get apex_apr_pool_parent_get

#ifdef apr_pool_is_ancestor
#undef apr_pool_is_ancestor
#endif
#define apr_pool_is_ancestor apex_apr_pool_is_ancestor

#ifdef apr_pool_tag
#undef apr_pool_tag
#endif
#define apr_pool_tag apex_apr_pool_tag

#ifdef apr_pool_num_bytes
#undef apr_pool_num_bytes
#endif
#define apr_pool_num_bytes apex_apr_pool_num_bytes

#ifdef apr_pool_userdata_set
#undef apr_pool_userdata_set
#endif
#define apr_pool_userdata_set apex_apr_pool_userdata_set

#ifdef apr_pool_userdata_setn
#undef apr_pool_userdata_setn
#endif
#define apr_pool_userdata_setn apex_apr_pool_userdata_setn

#ifdef apr_pool_userdata_get
#undef apr_pool_userdata_get
#endif
#define apr_pool_userdata_get apex_apr_pool_userdata_get

#ifdef apr_pool_cleanup_register
#undef apr_pool_cleanup_register
#endif
#define apr_pool_cleanup_register apex_apr_pool_cleanup_register

#ifdef apr_pool_pre_cleanup_register
#undef apr_pool_pre_cleanup_register
#endif
#define apr_pool_pre_cleanup_register apex_apr_pool_pre_cleanup_register

#ifdef apr_pool_cleanup_kill
#undef apr_pool_cleanup_kill
#endif
#define apr_pool_cleanup_kill apex_apr_pool_cleanup_kill

#ifdef apr_pool_child_cleanup_set
#undef apr_pool_child_cleanup_set
#endif
#define apr_pool_child_cleanup_set apex_apr_pool_child_cleanup_set

#ifdef apr_pool_cleanup_run
#undef apr_pool_cleanup_run
#endif
#define apr_pool_cleanup_run apex_apr_pool_cleanup_run

#ifdef apr_pool_cleanup_null
#undef apr_pool_cleanup_null
#endif
#define apr_pool_cleanup_null apex_apr_pool_cleanup_null

#ifdef apr_pool_cleanup_for_exec
#undef apr_pool_cleanup_for_exec
#endif
#define apr_pool_cleanup_for_exec apex_apr_pool_cleanup_for_exec

#ifdef apr_pool_find
#undef apr_pool_find
#endif
#define apr_pool_find apex_apr_pool_find

#ifdef apr_pool_join
#undef apr_pool_join
#endif
#define apr_pool_join apex_apr_pool_join

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_POOLS_H */

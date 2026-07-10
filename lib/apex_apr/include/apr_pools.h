#ifndef APEX_APR_APR_POOLS_H
#define APEX_APR_APR_POOLS_H

#include_next <apr_pools.h>

#ifdef USE_APEX_API

/*
 * Future APEX-backed pool skeleton.
 *
 * The fields are intentionally minimal placeholders for now:
 *   - parent: future hierarchy tracking
 *   - native_pool: bridge to system APR during the transition
 *   - allocator_state: future allocator/runtime-owned state
 *   - user_state: reserved for project-specific metadata
 *
 * Important: this skeleton is not wired in as the public apr_pool_t layout
 * yet. A large part of the codebase still passes apr_pool_t* into upstream APR
 * thread/socket APIs declared in system headers, so switching the public type
 * outright would require those APIs to be wrapped as one audited migration.
 */
typedef struct apex_apr_pool_layout {
	struct apex_apr_pool_layout *parent;
	void *native_pool;
	void *allocator_state;
	void *user_state;
} apex_apr_pool_layout;

/*
 * Keep apr_pool_t opaque at the shim boundary for now. The alias below gives
 * the project ownership of the public type name without breaking existing APR
 * calls that still rely on the upstream pool ABI.
 */
typedef apr_pool_t apex_apr_pool_t;
#define apr_pool_t apex_apr_pool_t

#ifdef __cplusplus
extern "C" {
#endif

/*
 * apr_pools.h is one of the easier transition points because most call sites
 * treat apr_pool_t as an opaque handle. Keep the type itself owned by system
 * APR for now and intercept only the lifecycle/allocation entry points.
 *
 * Future APEX migration plan:
 *   1. Introduce an APEX-backed pool or allocator object behind
 *      apex_apr_pool_layout.
 *   2. Replace create/destroy as a pair so ownership stays auditable.
 *   3. Wrap APR APIs that still accept apr_pool_t* before swapping the public
 *      apr_pool_t alias over to the custom layout.
 *   4. Move palloc/pcalloc once the allocator semantics are understood.
 */
apr_status_t apex_apr_pool_create(apr_pool_t **newpool, apr_pool_t *parent);
void apex_apr_pool_destroy(apr_pool_t *p);
void *apex_apr_palloc(apr_pool_t *p, apr_size_t size);
void *apex_apr_pcalloc(apr_pool_t *p, apr_size_t size);

#ifdef __cplusplus
}
#endif

#ifdef apr_pool_create
#undef apr_pool_create
#endif
#define apr_pool_create apex_apr_pool_create

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

#endif /* USE_APEX_API */

#endif /* APEX_APR_APR_POOLS_H */

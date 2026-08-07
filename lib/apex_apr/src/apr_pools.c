#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <apr_pools.h>

#ifdef USE_APEX_API
#undef apr_pool_initialize
#undef apr_pool_terminate
#undef apr_pool_create
#undef apr_pool_create_ex
#undef apr_pool_create_core_ex
#undef apr_pool_create_unmanaged_ex
#undef apr_pool_allocator_get
#undef apr_pool_clear
#undef apr_pool_destroy
#undef apr_palloc
#undef apr_pcalloc
#undef apr_pool_abort_set
#undef apr_pool_abort_get
#undef apr_pool_parent_get
#undef apr_pool_is_ancestor
#undef apr_pool_tag
#undef apr_pool_num_bytes
#undef apr_pool_userdata_set
#undef apr_pool_userdata_setn
#undef apr_pool_userdata_get
#undef apr_pool_cleanup_register
#undef apr_pool_pre_cleanup_register
#undef apr_pool_cleanup_kill
#undef apr_pool_child_cleanup_set
#undef apr_pool_cleanup_run
#undef apr_pool_cleanup_null
#undef apr_pool_cleanup_for_exec
#undef apr_pool_find
#undef apr_pool_join

#define APEX_APR_POOL_ARENA_DEFAULT_SIZE (1024u * 1024u)
#define APEX_APR_POOL_BLOCK_DEFAULT_SIZE 4096u
#define APEX_APR_POOL_CONTROL_CAPACITY 64u
#define APEX_APR_POOL_CLEANUP_CAPACITY 256u
#define APEX_APR_POOL_USERDATA_CAPACITY 128u
#define APEX_APR_POOL_ALIGNMENT ((apr_size_t)sizeof(void *))
#define APEX_APR_POOL_MUTEX_PRIORITY 1
#define APEX_APR_POOL_MUTEX_NAME "APEX_APR_POOL"

/* The fixed control table owns all public APEX pool objects. */
typedef apex_apr_pool_t apex_apr_pool_control;

static unsigned char *apex_apr_arena_base;
static apr_size_t apex_apr_arena_size;
static MUTEX_ID_TYPE apex_apr_pool_mutex_id;
static int apex_apr_pool_mutex_ready;
static int apex_apr_pool_memory_block_bound;
static apr_size_t apex_apr_arena_used;
static apex_apr_pool_block *apex_apr_free_blocks;
static apex_apr_pool_control apex_apr_pool_controls[APEX_APR_POOL_CONTROL_CAPACITY];
static apex_apr_pool_cleanup apex_apr_pool_cleanups[APEX_APR_POOL_CLEANUP_CAPACITY];
static apex_apr_pool_userdata apex_apr_pool_userdata_entries[APEX_APR_POOL_USERDATA_CAPACITY];

static void apex_apr_pool_lock(void)
{
	RETURN_CODE_TYPE return_code;

	if (!apex_apr_pool_mutex_ready) {
		return;
	}
	ACQUIRE_MUTEX(apex_apr_pool_mutex_id, INFINITE_TIME_VALUE, &return_code);
}

static void apex_apr_pool_unlock(void)
{
	RETURN_CODE_TYPE return_code;

	if (!apex_apr_pool_mutex_ready) {
		return;
	}
	RELEASE_MUTEX(apex_apr_pool_mutex_id, &return_code);
}

static apr_size_t apex_apr_pool_align(apr_size_t size)
{
	if (size > (apr_size_t)-1 - (APEX_APR_POOL_ALIGNMENT - 1)) {
		return 0;
	}
	return (size + APEX_APR_POOL_ALIGNMENT - 1) &
		~(APEX_APR_POOL_ALIGNMENT - 1);
}

static apex_apr_pool_control *apex_apr_pool_find_unlocked(const apr_pool_t *pool)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		if (apex_apr_pool_controls[i].active &&
		    &apex_apr_pool_controls[i] == pool) {
			return &apex_apr_pool_controls[i];
		}
	}
	return NULL;
}

static apex_apr_pool_control *apex_apr_pool_lookup(const apr_pool_t *pool)
{
	apex_apr_pool_control *control;

	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	apex_apr_pool_unlock();
	return control;
}

static apex_apr_pool_control *apex_apr_pool_control_create_unlocked(
	apr_pool_t *parent)
{
	unsigned int i;
	apex_apr_pool_control *control;
	apex_apr_pool_control *parent_control;

	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		if (!apex_apr_pool_controls[i].active) {
			control = &apex_apr_pool_controls[i];
			parent_control = apex_apr_pool_find_unlocked(parent);
			memset(control, 0, sizeof(*control));
			control->parent = parent_control;
			control->active = 1;
			if (parent_control != NULL) {
				control->next_sibling = parent_control->first_child;
				parent_control->first_child = control;
			}
			return control;
		}
	}
	return NULL;
}

static apex_apr_pool_block *apex_apr_pool_block_acquire_unlocked(apr_size_t minimum_size)
{
	apex_apr_pool_block **link;
	apex_apr_pool_block *block;
	apr_size_t capacity;
	apr_size_t header_size;
	apr_size_t total_size;

	capacity = minimum_size > APEX_APR_POOL_BLOCK_DEFAULT_SIZE ?
		minimum_size : APEX_APR_POOL_BLOCK_DEFAULT_SIZE;
	link = &apex_apr_free_blocks;
	while (*link != NULL) {
		if ((*link)->capacity >= capacity) {
			block = *link;
			*link = block->next;
			block->next = NULL;
			block->used = 0;
			block->from_host_heap = 0;
			return block;
		}
		link = &(*link)->next;
	}

	header_size = apex_apr_pool_align((apr_size_t)sizeof(*block));
	if (header_size == 0 || capacity > (apr_size_t)-1 - header_size) {
		return NULL;
	}
	total_size = header_size + capacity;
	if (total_size > apex_apr_arena_size - apex_apr_arena_used) {
		return NULL;
	}

	block = (apex_apr_pool_block *)(apex_apr_arena_base + apex_apr_arena_used);
	apex_apr_arena_used += total_size;
	block->from_host_heap = 0;
	block->next = NULL;
	block->capacity = capacity;
	block->used = 0;
	return block;
}

static void *apex_apr_pool_allocate_unlocked(apex_apr_pool_control *control,
									apr_size_t requested_size)
{
	apex_apr_pool_block *block;
	apr_size_t size;
	apr_size_t offset;

	size = requested_size == 0 ? 1 : requested_size;
	size = apex_apr_pool_align(size);
	if (size == 0) {
		return NULL;
	}
	block = control->blocks;
	if (block == NULL || size > block->capacity - block->used) {
		block = apex_apr_pool_block_acquire_unlocked(size);
		if (block == NULL) {
			return NULL;
		}
		block->next = control->blocks;
		control->blocks = block;
	}

	offset = apex_apr_pool_align(block->used);
	if (size > block->capacity - offset) {
		return NULL;
	}
	block->used = offset + size;
	return (unsigned char *)(block + 1) + offset;
}

static void apex_apr_pool_blocks_release_unlocked(apex_apr_pool_control *control)
{
	apex_apr_pool_block *block;

	block = control->blocks;
	while (block != NULL) {
		apex_apr_pool_block *next;

		next = block->next;
		block->next = apex_apr_free_blocks;
		block->used = 0;
		apex_apr_free_blocks = block;
		block = next;
	}
	control->blocks = NULL;
}

static apex_apr_pool_cleanup *apex_apr_pool_cleanup_acquire_unlocked(void)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_POOL_CLEANUP_CAPACITY; ++i) {
		if (!apex_apr_pool_cleanups[i].in_use) {
			memset(&apex_apr_pool_cleanups[i], 0,
			       sizeof(apex_apr_pool_cleanups[i]));
			apex_apr_pool_cleanups[i].in_use = 1;
			return &apex_apr_pool_cleanups[i];
		}
	}
	return NULL;
}

static void apex_apr_pool_cleanup_release_unlocked(apex_apr_pool_cleanup *cleanup)
{
	memset(cleanup, 0, sizeof(*cleanup));
}

static apex_apr_pool_userdata *apex_apr_pool_userdata_acquire_unlocked(void)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_POOL_USERDATA_CAPACITY; ++i) {
		if (!apex_apr_pool_userdata_entries[i].in_use) {
			memset(&apex_apr_pool_userdata_entries[i], 0,
			       sizeof(apex_apr_pool_userdata_entries[i]));
			apex_apr_pool_userdata_entries[i].in_use = 1;
			return &apex_apr_pool_userdata_entries[i];
		}
	}
	return NULL;
}

static void apex_apr_pool_userdata_release_list_unlocked(apex_apr_pool_control *control)
{
	apex_apr_pool_userdata *entry;

	entry = control->userdata;
	while (entry != NULL) {
		apex_apr_pool_userdata *next;

		next = entry->next;
		memset(entry, 0, sizeof(*entry));
		entry = next;
	}
	control->userdata = NULL;
}

static void apex_apr_pool_detach_unlocked(apex_apr_pool_control *control)
{
	apex_apr_pool_control **link;

	if (control->parent == NULL) {
		return;
	}
	for (link = &control->parent->first_child; *link != NULL; link = &(*link)->next_sibling) {
		if (*link == control) {
			*link = control->next_sibling;
			break;
		}
	}
	control->parent = NULL;
	control->next_sibling = NULL;
}

static void apex_apr_pool_run_cleanups(apex_apr_pool_control *control,
								   int pre_cleanup, int child_cleanup)
{
	apex_apr_pool_cleanup **list;

	list = pre_cleanup ? &control->pre_cleanups : &control->cleanups;
	for (;;) {
		apex_apr_pool_cleanup *cleanup;
		apr_status_t (*callback)(void *);
		const void *data;

		apex_apr_pool_lock();
		cleanup = *list;
		if (cleanup == NULL) {
			apex_apr_pool_unlock();
			return;
		}
		*list = cleanup->next;
		cleanup->next = NULL;
		callback = child_cleanup ? cleanup->child_cleanup : cleanup->plain_cleanup;
		data = cleanup->data;
		apex_apr_pool_unlock();

		if (callback != NULL) {
			(void)callback((void *)data);
		}

		apex_apr_pool_lock();
		apex_apr_pool_cleanup_release_unlocked(cleanup);
		apex_apr_pool_unlock();
	}
}

static apex_apr_pool_control *apex_apr_pool_first_child(apex_apr_pool_control *parent)
{
	apex_apr_pool_control *child;

	apex_apr_pool_lock();
	child = parent->first_child;
	apex_apr_pool_unlock();
	return child;
}

static void apex_apr_pool_control_destroy(apex_apr_pool_control *control);

static void apex_apr_pool_control_clear(apex_apr_pool_control *control)
{
	apex_apr_pool_control *child;

	apex_apr_pool_run_cleanups(control, 1, 0);
	while ((child = apex_apr_pool_first_child(control)) != NULL) {
		apex_apr_pool_control_destroy(child);
	}
	apex_apr_pool_run_cleanups(control, 0, 0);

	apex_apr_pool_lock();
	apex_apr_pool_blocks_release_unlocked(control);
	apex_apr_pool_userdata_release_list_unlocked(control);
	apex_apr_pool_unlock();

}

static void apex_apr_pool_control_destroy(apex_apr_pool_control *control)
{
	apex_apr_pool_control_clear(control);
	apex_apr_pool_lock();
	apex_apr_pool_detach_unlocked(control);
	memset(control, 0, sizeof(*control));
	apex_apr_pool_unlock();
}

apr_status_t apex_apr_pool_arena_configure(void *memory, apr_size_t size)
{
	unsigned int i;
	uintptr_t address;
	uintptr_t aligned_address;
	apr_size_t alignment_padding;

	if (memory == NULL || size < APEX_APR_POOL_BLOCK_DEFAULT_SIZE) {
		return APR_EINVAL;
	}
	apex_apr_pool_lock();
	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		if (apex_apr_pool_controls[i].active) {
			apex_apr_pool_unlock();
			return APR_EBUSY;
		}
	}

	address = (uintptr_t)memory;
	aligned_address = (address + APEX_APR_POOL_ALIGNMENT - 1) &
		~((uintptr_t)APEX_APR_POOL_ALIGNMENT - 1);
	alignment_padding = (apr_size_t)(aligned_address - address);
	if (alignment_padding >= size ||
	    size - alignment_padding < APEX_APR_POOL_BLOCK_DEFAULT_SIZE) {
		apex_apr_pool_unlock();
		return APR_EINVAL;
	}

	apex_apr_arena_base = (unsigned char *)aligned_address;
	apex_apr_arena_size = size - alignment_padding;
	apex_apr_arena_used = 0;
	apex_apr_free_blocks = NULL;
	apex_apr_pool_memory_block_bound = 1;
	apex_apr_pool_unlock();
	return APR_SUCCESS;
}

apr_status_t apex_apr_pool_arena_configure_memory_block(
	MEMORY_BLOCK_NAME_TYPE memory_block_name)
{
	(void)memory_block_name;
	return APR_ENOTIMPL;
}

apr_status_t apex_apr_pool_use_host_heap(int enabled)
{
	if (enabled) {
		return APR_ENOTIMPL;
	}
	return APR_SUCCESS;
}

int apex_apr_pool_uses_private_allocator(const apr_pool_t *pool)
{
	return apex_apr_pool_lookup(pool) != NULL;
}

apex_apr_pool_t *apex_apr_pool_get(const apr_pool_t *pool)
{
	return apex_apr_pool_lookup(pool);
}

apr_status_t apex_apr_pool_initialize(void)
{
	MUTEX_NAME_TYPE mutex_name = APEX_APR_POOL_MUTEX_NAME;
	RETURN_CODE_TYPE return_code;

	if (apex_apr_pool_mutex_ready) {
		return APR_SUCCESS;
	}
	CREATE_MUTEX(mutex_name, APEX_APR_POOL_MUTEX_PRIORITY, FIFO,
			     &apex_apr_pool_mutex_id, &return_code);
	if (return_code != NO_ERROR) {
		return APR_EGENERAL;
	}
	apex_apr_pool_mutex_ready = 1;
	return APR_SUCCESS;
}

void apex_apr_pool_terminate(void)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		if (apex_apr_pool_controls[i].active &&
		    apex_apr_pool_controls[i].parent == NULL &&
		    !apex_apr_pool_controls[i].unmanaged) {
			apex_apr_pool_control_destroy(&apex_apr_pool_controls[i]);
		}
	}
}

apr_status_t apex_apr_pool_create_ex(apr_pool_t **newpool, apr_pool_t *parent,
						 apr_abortfunc_t abort_fn, apr_allocator_t *allocator)
{
	apex_apr_pool_control *control;

	if (!apex_apr_pool_mutex_ready || !apex_apr_pool_memory_block_bound) {
		return APR_EGENERAL;
	}
	if (newpool == NULL || (parent != NULL && apex_apr_pool_lookup(parent) == NULL)) {
		return APR_EINVAL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_control_create_unlocked(parent);
	apex_apr_pool_unlock();
	if (control == NULL) {
		return APR_ENOMEM;
	}
	if (allocator != NULL) {
		apex_apr_pool_control_destroy(control);
		return APR_ENOTIMPL;
	}
	control->abort_fn = abort_fn != NULL ? abort_fn :
		(parent != NULL ? apex_apr_pool_abort_get(parent) : NULL);
	*newpool = control;
	return APR_SUCCESS;
}

apr_status_t apex_apr_pool_create(apr_pool_t **newpool, apr_pool_t *parent)
{
	return apex_apr_pool_create_ex(newpool, parent, NULL, NULL);
}

apr_status_t apex_apr_pool_create_core_ex(apr_pool_t **newpool,
						      apr_abortfunc_t abort_fn, apr_allocator_t *allocator)
{
	return apex_apr_pool_create_unmanaged_ex(newpool, abort_fn, allocator);
}

apr_status_t apex_apr_pool_create_unmanaged_ex(apr_pool_t **newpool,
						       apr_abortfunc_t abort_fn, apr_allocator_t *allocator)
{
	apr_status_t status;

	status = apex_apr_pool_create_ex(newpool, NULL, abort_fn, allocator);
	if (status == APR_SUCCESS) {
		(*newpool)->unmanaged = 1;
	}
	return status;
}

apr_allocator_t *apex_apr_pool_allocator_get(apr_pool_t *pool)
{
	(void)pool;
	return NULL;
}

void apex_apr_pool_clear(apr_pool_t *p)
{
	apex_apr_pool_control *control;

	if (p == NULL) {
		return;
	}
	control = apex_apr_pool_lookup(p);
	if (control != NULL) {
		apex_apr_pool_control_clear(control);
	}
}

void apex_apr_pool_destroy(apr_pool_t *p)
{
	apex_apr_pool_control *control;

	if (p == NULL) {
		return;
	}
	control = apex_apr_pool_lookup(p);
	if (control != NULL) {
		apex_apr_pool_control_destroy(control);
	}
}

void *apex_apr_palloc(apr_pool_t *p, apr_size_t size)
{
	apex_apr_pool_control *control;
	apr_abortfunc_t abort_fn;
	void *memory;

	if (p == NULL) {
		return NULL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(p);
	if (control != NULL) {
		memory = apex_apr_pool_allocate_unlocked(control, size);
		abort_fn = control->abort_fn;
		apex_apr_pool_unlock();
		if (memory == NULL && abort_fn != NULL) {
			(void)abort_fn(APR_ENOMEM);
		}
		return memory;
	}
	apex_apr_pool_unlock();
	return NULL;
}

void *apex_apr_pcalloc(apr_pool_t *p, apr_size_t size)
{
	void *mem;

	if (p == NULL) {
		return NULL;
	}

	mem = apex_apr_palloc(p, size);
	if (mem != NULL) {
		memset(mem, 0, size);
	}
	return mem;
}

void apex_apr_pool_abort_set(apr_abortfunc_t abortfunc, apr_pool_t *pool)
{
	apex_apr_pool_control *control;

	if (pool == NULL) {
		return;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	if (control != NULL) {
		control->abort_fn = abortfunc;
	}
	apex_apr_pool_unlock();
}

apr_abortfunc_t apex_apr_pool_abort_get(apr_pool_t *pool)
{
	apex_apr_pool_control *control;
	apr_abortfunc_t abort_fn;

	if (pool == NULL) {
		return NULL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	abort_fn = control != NULL ? control->abort_fn : NULL;
	apex_apr_pool_unlock();
	return abort_fn;
}

apr_pool_t *apex_apr_pool_parent_get(apr_pool_t *pool)
{
	apex_apr_pool_control *control;
	apr_pool_t *parent;

	if (pool == NULL) {
		return NULL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	parent = control != NULL ? control->parent : NULL;
	apex_apr_pool_unlock();
	return parent;
}

int apex_apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b)
{
	apex_apr_pool_control *control;
	int result;

	if (b == NULL) {
		return 0;
	}
	if (a == NULL) {
		return 1;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(b);
	result = 0;
	while (control != NULL) {
		if (control == a) {
			result = 1;
			break;
		}
		control = control->parent;
	}
	apex_apr_pool_unlock();
	return result;
}

void apex_apr_pool_tag(apr_pool_t *pool, const char *tag)
{
	apex_apr_pool_control *control;

	if (pool == NULL) {
		return;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	if (control != NULL) {
		control->tag = tag;
	}
	apex_apr_pool_unlock();
}

apr_size_t apex_apr_pool_num_bytes(apr_pool_t *pool, int recurse)
{
	apex_apr_pool_control *control;
	apr_size_t total;
	unsigned int i;

	if (pool == NULL) {
		return 0;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	if (control == NULL) {
		apex_apr_pool_unlock();
		return 0;
	}
	total = 0;
	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		apex_apr_pool_block *block;
		apex_apr_pool_control *candidate;

		if (!apex_apr_pool_controls[i].active ||
		    (!recurse && &apex_apr_pool_controls[i] != control)) {
			continue;
		}
		if (recurse) {
			candidate = &apex_apr_pool_controls[i];
			while (candidate != NULL && candidate != control) {
				candidate = candidate->parent;
			}
			if (candidate == NULL) {
				continue;
			}
		}
		for (block = apex_apr_pool_controls[i].blocks; block != NULL;
		     block = block->next) {
			total += block->used;
		}
	}
	apex_apr_pool_unlock();
	return total;
}

static apr_status_t apex_apr_pool_userdata_set_internal(const void *data,
	const char *key, apr_status_t (*cleanup)(void *), apr_pool_t *pool, int copy_key)
{
	apex_apr_pool_control *control;
	apex_apr_pool_userdata *entry;
	char *key_copy;

	if (key == NULL || pool == NULL) {
		return APR_EINVAL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	if (control == NULL) {
		apex_apr_pool_unlock();
		return APR_EINVAL;
	}
	for (entry = control->userdata; entry != NULL; entry = entry->next) {
		if (strcmp(entry->key, key) == 0) {
			entry->data = data;
			apex_apr_pool_unlock();
			if (cleanup != NULL) {
				apex_apr_pool_cleanup_register(pool, data, cleanup, cleanup);
			}
			return APR_SUCCESS;
		}
	}
	entry = apex_apr_pool_userdata_acquire_unlocked();
	apex_apr_pool_unlock();
	if (entry == NULL) {
		return APR_ENOMEM;
	}

	key_copy = (char *)key;
	if (copy_key) {
		apr_size_t key_length = (apr_size_t)strlen(key) + 1;

		key_copy = apex_apr_palloc(pool, key_length);
		if (key_copy == NULL) {
			apex_apr_pool_lock();
			memset(entry, 0, sizeof(*entry));
			apex_apr_pool_unlock();
			return APR_ENOMEM;
		}
		memcpy(key_copy, key, key_length);
	}

	apex_apr_pool_lock();
	entry->data = data;
	entry->key = key_copy;
	entry->next = control->userdata;
	control->userdata = entry;
	apex_apr_pool_unlock();
	if (cleanup != NULL) {
		apex_apr_pool_cleanup_register(pool, data, cleanup, cleanup);
	}
	return APR_SUCCESS;
}

apr_status_t apex_apr_pool_userdata_set(const void *data, const char *key,
						apr_status_t (*cleanup)(void *), apr_pool_t *pool)
{
	return apex_apr_pool_userdata_set_internal(data, key, cleanup, pool, 1);
}

apr_status_t apex_apr_pool_userdata_setn(const void *data, const char *key,
						 apr_status_t (*cleanup)(void *), apr_pool_t *pool)
{
	return apex_apr_pool_userdata_set_internal(data, key, cleanup, pool, 0);
}

apr_status_t apex_apr_pool_userdata_get(void **data, const char *key, apr_pool_t *pool)
{
	apex_apr_pool_control *control;
	apex_apr_pool_userdata *entry;

	if (data == NULL || key == NULL || pool == NULL) {
		return APR_EINVAL;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(pool);
	if (control == NULL) {
		apex_apr_pool_unlock();
		return APR_EINVAL;
	}
	for (entry = control->userdata; entry != NULL; entry = entry->next) {
		if (strcmp(entry->key, key) == 0) {
			*data = (void *)entry->data;
			apex_apr_pool_unlock();
			return APR_SUCCESS;
		}
	}
	*data = NULL;
	apex_apr_pool_unlock();
	return APR_SUCCESS;
}

static void apex_apr_pool_cleanup_register_internal(apr_pool_t *p, const void *data,
	apr_status_t (*plain_cleanup)(void *), apr_status_t (*child_cleanup)(void *), int pre_cleanup)
{
	apex_apr_pool_control *control;
	apex_apr_pool_cleanup *cleanup;

	if (p == NULL || plain_cleanup == NULL || (!pre_cleanup && child_cleanup == NULL)) {
		return;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(p);
	if (control == NULL) {
		apex_apr_pool_unlock();
		return;
	}
	cleanup = apex_apr_pool_cleanup_acquire_unlocked();
	if (cleanup != NULL) {
		cleanup->data = data;
		cleanup->plain_cleanup = plain_cleanup;
		cleanup->child_cleanup = child_cleanup;
		if (pre_cleanup) {
			cleanup->next = control->pre_cleanups;
			control->pre_cleanups = cleanup;
		} else {
			cleanup->next = control->cleanups;
			control->cleanups = cleanup;
		}
	}
	apex_apr_pool_unlock();
	if (cleanup == NULL) {
		apr_abortfunc_t abort_fn = apex_apr_pool_abort_get(p);

		if (abort_fn != NULL) {
			(void)abort_fn(APR_ENOMEM);
		}
	}
}

void apex_apr_pool_cleanup_register(apr_pool_t *p, const void *data,
						 apr_status_t (*plain_cleanup)(void *),
						 apr_status_t (*child_cleanup)(void *))
{
	apex_apr_pool_cleanup_register_internal(p, data, plain_cleanup, child_cleanup, 0);
}

void apex_apr_pool_pre_cleanup_register(apr_pool_t *p, const void *data,
						     apr_status_t (*plain_cleanup)(void *))
{
	apex_apr_pool_cleanup_register_internal(p, data, plain_cleanup, NULL, 1);
}

static void apex_apr_pool_cleanup_kill_list_unlocked(apex_apr_pool_cleanup **list,
	const void *data, apr_status_t (*cleanup)(void *))
{
	apex_apr_pool_cleanup **link;

	for (link = list; *link != NULL; link = &(*link)->next) {
		if ((*link)->data == data && (*link)->plain_cleanup == cleanup) {
			apex_apr_pool_cleanup *matched = *link;

			*link = matched->next;
			apex_apr_pool_cleanup_release_unlocked(matched);
			return;
		}
	}
}

void apex_apr_pool_cleanup_kill(apr_pool_t *p, const void *data,
						 apr_status_t (*cleanup)(void *))
{
	apex_apr_pool_control *control;

	if (p == NULL || cleanup == NULL) {
		return;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(p);
	if (control != NULL) {
		apex_apr_pool_cleanup_kill_list_unlocked(&control->cleanups, data, cleanup);
		apex_apr_pool_cleanup_kill_list_unlocked(&control->pre_cleanups, data, cleanup);
		apex_apr_pool_unlock();
		return;
	}
	apex_apr_pool_unlock();
}

void apex_apr_pool_child_cleanup_set(apr_pool_t *p, const void *data,
						  apr_status_t (*plain_cleanup)(void *),
						  apr_status_t (*child_cleanup)(void *))
{
	apex_apr_pool_control *control;
	apex_apr_pool_cleanup *cleanup;

	if (p == NULL || plain_cleanup == NULL || child_cleanup == NULL) {
		return;
	}
	apex_apr_pool_lock();
	control = apex_apr_pool_find_unlocked(p);
	if (control != NULL) {
		for (cleanup = control->cleanups; cleanup != NULL; cleanup = cleanup->next) {
			if (cleanup->data == data && cleanup->plain_cleanup == plain_cleanup) {
				cleanup->child_cleanup = child_cleanup;
				break;
			}
		}
		apex_apr_pool_unlock();
		return;
	}
	apex_apr_pool_unlock();
}

apr_status_t apex_apr_pool_cleanup_run(apr_pool_t *p, void *data,
						   apr_status_t (*cleanup)(void *))
{
	if (cleanup == NULL) {
		return APR_EINVAL;
	}
	apex_apr_pool_cleanup_kill(p, data, cleanup);
	return cleanup(data);
}

apr_status_t apex_apr_pool_cleanup_null(void *data)
{
	(void)data;
	return APR_SUCCESS;
}

void apex_apr_pool_cleanup_for_exec(void)
{
	unsigned int i;

	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		apex_apr_pool_control *control;

		apex_apr_pool_lock();
		control = apex_apr_pool_controls[i].active ? &apex_apr_pool_controls[i] : NULL;
		apex_apr_pool_unlock();
		if (control != NULL) {
			apex_apr_pool_run_cleanups(control, 0, 1);
		}
	}
}

apr_pool_t *apex_apr_pool_find(const void *mem)
{
	unsigned int i;

	if (mem == NULL) {
		return NULL;
	}
	apex_apr_pool_lock();
	for (i = 0; i < APEX_APR_POOL_CONTROL_CAPACITY; ++i) {
		apex_apr_pool_block *block;

		if (!apex_apr_pool_controls[i].active) {
			continue;
		}
		for (block = apex_apr_pool_controls[i].blocks; block != NULL;
		     block = block->next) {
			const unsigned char *begin = (const unsigned char *)(block + 1);
			const unsigned char *end = begin + block->used;

			if ((const unsigned char *)mem >= begin &&
			    (const unsigned char *)mem < end) {
				apex_apr_pool_unlock();
				return &apex_apr_pool_controls[i];
			}
		}
	}
	apex_apr_pool_unlock();
	return NULL;
}

void apex_apr_pool_join(apr_pool_t *pool, apr_pool_t *sub)
{
	(void)pool;
	(void)sub;
}

#endif /* USE_APEX_API */

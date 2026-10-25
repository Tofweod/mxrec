/* Test-only allocation accounting for code and static libraries linked with
 * --wrap. Shared-library internals (for example stdio buffers) are excluded.
 * Keep the registry static so the checker never allocates recursively. */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void *__real_reallocarray(void *, size_t, size_t);
char *__real_strdup(const char *);
char *__real_strndup(const char *, size_t);
void __real_free(void *);

static void *allocations[4096];
static size_t live;

static void remember(void *ptr)
{
	if (!ptr)
		return;
	for (size_t i = 0; i < sizeof(allocations) / sizeof(*allocations); i++) {
		if (!allocations[i]) {
			allocations[i] = ptr;
			live++;
			return;
		}
	}
	fputs("allocation checker registry exhausted\n", stderr);
	abort();
}

static void forget(void *ptr)
{
	if (!ptr)
		return;
	for (size_t i = 0; i < sizeof(allocations) / sizeof(*allocations); i++) {
		if (allocations[i] == ptr) {
			allocations[i] = NULL;
			live--;
			return;
		}
	}
	fputs("free/realloc of an untracked allocation\n", stderr);
	abort();
}

void *__wrap_malloc(size_t size)
{
	void *ptr = __real_malloc(size);
	remember(ptr);
	return ptr;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *ptr = __real_calloc(count, size);
	remember(ptr);
	return ptr;
}

void *__wrap_realloc(void *ptr, size_t size)
{
	forget(ptr);
	void *next = __real_realloc(ptr, size);
	remember(next || !size ? next : ptr);
	return next;
}

void *__wrap_reallocarray(void *ptr, size_t count, size_t size)
{
	forget(ptr);
	void *next = __real_reallocarray(ptr, count, size);
	remember(next || !count || !size ? next : ptr);
	return next;
}

char *__wrap_strdup(const char *str)
{
	char *ptr = __real_strdup(str);
	remember(ptr);
	return ptr;
}

char *__wrap_strndup(const char *str, size_t len)
{
	char *ptr = __real_strndup(str, len);
	remember(ptr);
	return ptr;
}

void __wrap_free(void *ptr)
{
	forget(ptr);
	__real_free(ptr);
}

__attribute__((destructor)) static void check_allocations(void)
{
	if (live) {
		fprintf(stderr, "allocation checker: %zu unfreed allocations\n", live);
		_Exit(99);
	}
	puts("allocation checker: no outstanding allocations");
}

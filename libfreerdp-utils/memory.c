/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Memory Utils
 *
 * Copyright 2009-2014 Jay Sorg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <freerdp/utils/memory.h>

#define MEMORY_MAX_ALLOC (64 * 1024 * 1024)
/**
 * Allocate memory.
 * @param size
 */

static int check_memory(const char* func, int size)
{
	if (size < 1)
	{
		printf("check_memory: func %s adjusting bytes %d\n", func, size);
		size = 1;
	}
	if (size > MEMORY_MAX_ALLOC)
	{
		printf("check_memory: func %s size %d\n", func, size);
		return 1;
	}
	return 0;
}

void* xmalloc(size_t size)
{
	void* mem;

	if (check_memory("xmalloc", size))
		return NULL;
	mem = malloc(size);
	if (mem == NULL)
	{
		perror("xmalloc");
		printf("xmalloc: failed to allocate memory of size: %d\n", (int) size);
	}
	return mem;
}

/**
 * Allocate memory initialized to zero.
 * @param size
 */

void* xzalloc(size_t size)
{
	void* mem;

	if (check_memory("xzalloc", size))
		return NULL;
	mem = calloc(1, size);
	if (mem == NULL)
	{
		perror("xzalloc");
		printf("xzalloc: failed to allocate memory of size: %d\n", (int) size);
	}
	return mem;
}

/**
 * Reallocate memory.
 * @param ptr
 * @param size
 */

void* xrealloc(void* ptr, size_t size)
{
	void* mem;

	if (check_memory("xrealloc", size))
		return NULL;
	if (ptr == NULL)
	{
		printf("xrealloc: null pointer given\n");
		return NULL;
	}
	mem = realloc(ptr, size);
	if (mem == NULL)
	{
		perror("xrealloc");
		printf("xrealloc: failed to allocate memory of size: %d\n", (int) size);
	}
	return mem;
}

void* xrealloc_check(void* ptr, size_t size)
{
	if (size < 1)
	{
		if (ptr != NULL)
		{
			free(ptr);
		}
		return NULL;
	}
	if (ptr == NULL)
	{
		return malloc(size);
	}
	return realloc(ptr, size);
}

/**
 * Free memory.
 * @param mem
 */

void xfree(void* ptr)
{
	if (ptr != NULL)
		free(ptr);
}

/**
 * Duplicate a string in memory.
 * @param str
 * @return
 */

char* xstrdup(const char* str)
{
	char* mem;

	if (str == NULL)
		return NULL;

#ifdef _WIN32
	mem = _strdup(str);
#else
	mem = strdup(str);
#endif

	if (mem == NULL)
		perror("strdup");

	return mem;
}

struct shm_info_t* create_shm_info(size_t size)
{
	struct shm_info_t* rv;

	rv = xnew(struct shm_info_t);
	rv->bytes = (int)size;
#ifdef _WIN32
	rv->ptr = xmalloc(size);
#else
	rv->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
	rv->ptr = shmat(rv->shmid, 0, 0);
#endif
	return rv;
}

void delete_shm_info(struct shm_info_t* shm_info)
{
	if (shm_info == NULL)
		return;
#ifdef _WIN32
	xfree(shm_info->ptr);
#else
	shmdt(shm_info->ptr);
	shmctl(shm_info->shmid, IPC_RMID, NULL);
#endif
	xfree(shm_info);
}

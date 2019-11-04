/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Memory Utils
 *
 * Copyright 2009-2013 Jay Sorg
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

#ifndef __MEMORY_UTILS_H
#define __MEMORY_UTILS_H

#include <stddef.h>
#include <freerdp/api.h>

struct shm_info_t
{
	int shmid;
	int bytes;
	void* ptr;
};

FREERDP_API void* xmalloc(size_t size);
FREERDP_API void* xzalloc(size_t size);
FREERDP_API void* xrealloc(void* ptr, size_t size);
FREERDP_API void* xrealloc_check(void* ptr, size_t size);
FREERDP_API void xfree(void* ptr);
FREERDP_API char* xstrdup(const char* str);

FREERDP_API struct shm_info_t* create_shm_info(size_t size);
FREERDP_API void delete_shm_info(struct shm_info_t* shm_info);

#define xnew(_type) (_type*)xzalloc(sizeof(_type))
#define xnew0(_type, _count) (_type*)calloc(_count, sizeof(_type))
#define xrenew(_type, _ptr, _count) (_type*)xrealloc_check(_ptr, sizeof(_type) * (_count))

#endif /* __MEMORY_UTILS_H */

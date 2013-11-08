/**
 * NeutrinoRDP: A Remote Desktop Protocol Client
 * pointer
 *
 * Copyright 2013 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __POINTER1_H
#define __POINTER1_H

#include "rdp.h"
#include "orders.h"
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

rdpPointerUpdate* pointer_new(void);
void pointer_free(rdpPointerUpdate* self);

#endif /* __POINTER1_H */

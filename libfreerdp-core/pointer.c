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

#include "update.h"
#include "surface.h"
#include <freerdp/utils/rect.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/types.h>
#include <freerdp/pointer.h>
#include "pointer.h"

/* dummy functions */
void defPointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointer_position)
{
}

void defPointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer_system)
{
}

void defPointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointer_color)
{
}

void defPointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointer_new)
{
}

void defPointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointer_cached)
{
}

rdpPointerUpdate* pointer_new(void)
{
	rdpPointerUpdate* rv;

	rv = xnew(rdpPointerUpdate);
	rv->PointerPosition = defPointerPosition;
	rv->PointerSystem = defPointerSystem;
	rv->PointerColor = defPointerColor;
	rv->PointerNew = defPointerNew;
	rv->PointerCached = defPointerCached;
	return rv;
}

void pointer_free(rdpPointerUpdate* self)
{
	xfree(self);
}

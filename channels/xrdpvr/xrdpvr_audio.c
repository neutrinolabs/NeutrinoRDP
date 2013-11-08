/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel - Audio Device Manager
 *
 * Copyright 2012 Laxmikant Rashinkar
 * Copyright 2010-2011 Vic Lee
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
#include <freerdp/utils/memory.h>
#include <freerdp/utils/load_plugin.h>

#include "xrdpvr_audio.h"

static XrdpvrAudioDevice *xrdpvr_load_audio_device_by_name(const char *name, const char *device)
{
	XrdpvrAudioDevice *audio;
	XRDPVR_AUDIO_DEVICE_ENTRY entry;
	char *fullname;

	if (strrchr(name, '.') != NULL)
	{
		//printf("name %s\n", name);
		entry = (XRDPVR_AUDIO_DEVICE_ENTRY) freerdp_load_plugin(name, XRDPVR_AUDIO_DEVICE_EXPORT_FUNC_NAME);
	}
	else
	{
		fullname = xzalloc(strlen(name) + 8);
		strcpy(fullname, "xrdpvr_");
		strcat(fullname, name);
		//printf("fullname %s\n", fullname);
		entry = (XRDPVR_AUDIO_DEVICE_ENTRY) freerdp_load_plugin(fullname, XRDPVR_AUDIO_DEVICE_EXPORT_FUNC_NAME);
		xfree(fullname);
	}

	if (entry == NULL)
	{
		//printf("error-------------------------\n");
		return NULL;
	}

	audio = entry();

	if (audio == NULL)
	{
		DEBUG_WARN("failed to call export function in %s", name);
		return NULL;
	}

	if (!audio->Open(audio, device))
	{
		//printf("error-------------------------\n");
		audio->Free(audio);
		audio = NULL;
	}

	return audio;
}

XrdpvrAudioDevice *xrdpvr_load_audio_device(const char *name, const char *device)
{
	XrdpvrAudioDevice *audio;

	if (name)
	{
		audio = xrdpvr_load_audio_device_by_name(name, device);
	}
	else
	{
		//audio = xrdpvr_load_audio_device_by_name("pulse", device);
		//if (!audio)
		{
			audio = xrdpvr_load_audio_device_by_name("alsa", device);
		}
	}

	return audio;
}

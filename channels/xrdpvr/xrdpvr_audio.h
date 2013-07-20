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

#ifndef __XRDPVR_AUDIO_H
#define __XRDPVR_AUDIO_H

#include "drdynvc_types.h"

typedef struct _XrdpvrAudioDevice XrdpvrAudioDevice;

struct _XrdpvrAudioDevice
{
	/* Open the audio device. */
	boolean (*Open) (XrdpvrAudioDevice *audio, const char *device);

	/* Set the audio data format. */
	boolean (*SetFormat) (XrdpvrAudioDevice *audio, uint32 sample_rate, uint32 channels, uint32 bits_per_sample);

	/* Play audio data. */
	boolean (*Play) (XrdpvrAudioDevice *audio, uint8 *data, uint32 data_size);

	/* Get the latency of the last written sample, in 100ns */
	uint64 (*GetLatency) (XrdpvrAudioDevice *audio);

	/* Flush queued audio data */
	void (*Flush) (XrdpvrAudioDevice *audio);

	/* Set the audio volume */
	void (*SetVolume) (XrdpvrAudioDevice *audio, int volume);

	/* Free the audio device */
	void (*Free) (XrdpvrAudioDevice *audio);
};

#define XRDPVR_AUDIO_DEVICE_EXPORT_FUNC_NAME "XrdpvrAudioDeviceEntry"
typedef XrdpvrAudioDevice *(*XRDPVR_AUDIO_DEVICE_ENTRY) (void);

XrdpvrAudioDevice *xrdpvr_load_audio_device(const char *name, const char *device);

#endif

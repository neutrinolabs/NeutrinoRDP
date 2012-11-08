/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Xrdp video redirection channel
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/event.h>
#include <freerdp/plugins/tsmf.h>

#include "xrdpvr_main.h"
#include "xrdpvr_audio.h"
#include "xrdpvr_player.h"

struct xrdpvr_plugin
{
	rdpSvcPlugin       plugin;
	XrdpvrAudioDevice* audio_device;
	STREAM*        	   s;
	void*              decoder; /* reference to XrdpMpegDecoder */
	int                got_partial_data;
	int                bytes_needed;
	int                audio_inited;
};

void* g_psi = NULL;     /* player state info    */
int   g_meta_data_fd;   /* media meta data file */

#if 0
static void xrdpvr_process_interval(rdpSvcPlugin *plugin)
{
	printf("xrdpvr: xrdpvr_process_interval:\n");
}
#endif

/*
 ******************************************************************************/
static void xrdpvr_process_receive(rdpSvcPlugin *plugin, STREAM *data_in)
{
	xrdpvrPlugin*   xrdpvr = (xrdpvrPlugin *) plugin;
	STREAM*         s;
	int             rv;
	int             bytes_to_process;
	int             i;
	uint32          cmd;
	uint32          data_len;
	uint32          tmp;
	uint8*          decoded_data;
	uint32          uncompressed_size;

	if (xrdpvr == NULL)
	{
		stream_free(data_in);
		return;
	}

	if (stream_get_size(data_in) <= 0)
	{
		stream_free(data_in);
		return;
	}

start:

	if (xrdpvr->got_partial_data)
	{
		/* previous pkt was fragmented and could not be processed; */
		/* if we have enough data, process it now, else defer      */
		/* processing till next pkt                                */

		/* how much additional data did we get? */
		int data_in_len = data_in->size;

#ifdef DEBUG_FRAGMENTS
		printf("###     partial_pkt has %d bytes\n", data_in_len);
#endif
		/* append new data to old */
		memcpy(xrdpvr->s->p, data_in->p, data_in_len);
		xrdpvr->s->p += data_in_len;

		if (data_in_len < xrdpvr->bytes_needed)
		{
			/* we still don't have enough data */
			xrdpvr->bytes_needed -= data_in_len;
			stream_free(data_in);
			return;
		}

		/* we have enough data */
		xrdpvr->bytes_needed = 0;
		xrdpvr->got_partial_data = 0;
		bytes_to_process = stream_get_length(xrdpvr->s);
		stream_set_pos(xrdpvr->s, 4); /* point to cmd */
		s = xrdpvr->s;

#ifdef DEBUG_FRAGMENTS
		printf("###     pkt complete; bytes_to_process=%d bytes\n", bytes_to_process);
#endif
	}
	else
	{
		stream_read_uint32(data_in, data_len);

		if ((data_in->size - 4) < data_len)
		{
#ifdef DEBUG_FRAGMENTS
			printf("###     pkt is fragmented; this pkt has %d bytes\n", data_len);
#endif

			/* got a fragmented pkt - save it for later processing */
			xrdpvr->s->p = xrdpvr->s->data;
			memcpy(xrdpvr->s->p, data_in->data, data_in->size);
			xrdpvr->s->p += data_in->size;
			xrdpvr->got_partial_data = 1;
			xrdpvr->bytes_needed = data_len - (data_in->size - 4);
			stream_free(data_in);
			return;
		}

		/* got complete pkt */
#ifdef DEBUG_FRAGMENTS
		printf("### pkt is NOT fragmented and has %d bytes\n", data_len);
#endif
		xrdpvr->got_partial_data = 0;
		xrdpvr->bytes_needed = 0;
		s = data_in;
	}

	stream_read_uint32(s, cmd);

	switch (cmd)
	{
		case CMD_SEND_AUDIO_DATA:
			stream_read_uint32(s, tmp); /* stream id */
			stream_read_uint32(s, data_len);

			/* send decoded data to ALSA */
			if (xrdpvr->audio_inited)
			{
				/* TODO check for return */
				process_audio(g_psi, s->p, data_len);
				s->p += data_len;

				decoded_data = get_decoded_audio_data(g_psi,
				                                      &uncompressed_size);

				if ((decoded_data == NULL) ||
				        (uncompressed_size == 0))
				{
					break;
				}

				(xrdpvr->audio_device->Play)(xrdpvr->audio_device,
				                             decoded_data,
				                             uncompressed_size);
			}

			break;

		case CMD_SEND_VIDEO_DATA:
			stream_read_uint32(s, tmp); /* stream id */
			stream_read_uint32(s, data_len);

			/* TODO check for return */
			process_video(g_psi, s->p, data_len);
			s->p += data_len;

			/* TODO do we need this: av_free_packet(&av_pkt); */
			break;

		case CMD_SET_VIDEO_FORMAT:
			stream_read_uint32(s, tmp); /* stream id */
			g_psi = init_player((void *) plugin, META_DATA_FILEAME);

			if (g_psi == NULL)
			{
				printf("init_player() failed\n");
			}

			break;

		case CMD_SET_AUDIO_FORMAT:
			stream_read_uint32(s, tmp); /* stream id */

			if (xrdpvr->audio_inited)
			{
				int samp_per_sec;
				int num_channels;
				int bits_per_samp;

				get_audio_config(g_psi, &samp_per_sec,
				                 &num_channels, &bits_per_samp);

				rv = (xrdpvr->audio_device->SetFormat)(xrdpvr->audio_device,
				                                       samp_per_sec,
				                                       num_channels,
				                                       bits_per_samp);

				if (!rv)
				{
					DEBUG_WARN("ERROR setting audio format\n");
				}
			}

			break;

		case CMD_CREATE_META_DATA_FILE:

			if ((g_meta_data_fd = open(META_DATA_FILEAME,
			                           O_RDWR | O_CREAT | O_TRUNC,
			                           0755)) < 0)
			{
				DEBUG_WARN("ERROR opening %s; "
				           "video redirection disabled!\n",
				           META_DATA_FILEAME);
			}

			break;

		case CMD_CLOSE_META_DATA_FILE:
			close(g_meta_data_fd);
			break;

		case CMD_WRITE_META_DATA:
			stream_read_uint32(s, data_len);

			if ((rv = write(g_meta_data_fd, s->p, data_len)) != data_len)
			{
				close(g_meta_data_fd);
				g_meta_data_fd = -1;
				DEBUG_WARN("ERROR writing to %s; "
				           "video redirection disabled!\n",
				           META_DATA_FILEAME);
			}

			s->p += data_len;
			break;

		case CMD_DEINIT_XRDPVR:
			stream_read_uint32(s, tmp); /* stream id */
			deinit_player(g_psi);
			break;

		default:
			printf("### got unknown command 0x%x %d(.)\n", cmd, cmd);
			break;
	}

	if (s == xrdpvr->s)
	{
		/* we just finished processing a fragmented pkt; */
		/* did we get part of the next command as well?  */

		i = stream_get_length(xrdpvr->s);

		if (bytes_to_process > i)
		{
			/* yes we did - copy this to data_in and start all over */
#ifdef DEBUG_FRAGMENTS
			printf("### xrdpvr_process_receive: got part of next command\n");
			printf("### xrdpvr_process_receive: bytes_to_process=%d diff=%d\n",
			       bytes_to_process, bytes_to_process - i);
#endif
			memcpy(data_in->data, s->p, bytes_to_process - i);
			data_in->p = data_in->data;
			xrdpvr->got_partial_data = 0;
			xrdpvr->bytes_needed = 0;
			bytes_to_process = 0;
			goto start;
		}
	}

	stream_free(data_in);
}

/*
 ******************************************************************************/
static void xrdpvr_process_connect(rdpSvcPlugin *plugin_p)
{
	xrdpvrPlugin *plugin = (xrdpvrPlugin *) plugin_p;

	if (plugin == NULL)
	{
		return;
	}

#if 0
	/* if you want a call from channel thread once a second, do this */
	plugin_p->interval_ms = 1000000;
	plugin_p->interval_callback = xrdpvr_process_interval;
#endif

	/* setup stream */
	plugin->s = stream_new(1024 * 1024);
	plugin->got_partial_data = 0;
	plugin->bytes_needed = 0;

	/* setup audio */
	plugin->audio_device = xrdpvr_load_audio_device("alsa", NULL);

	if (plugin->audio_device == NULL)
	{
		DEBUG_WARN("xrdpvr: error loading audio plugin; "
		           "audio will not be redirected");
		return;
	}

	plugin->audio_inited = (plugin->audio_device->Open)(plugin->audio_device, NULL);

	if (!plugin->audio_inited)
	{
		DEBUG_WARN("xrdpvr: failed to init ALSA; audio will not be redirected");
	}
}

/*
 ******************************************************************************/
static void xrdpvr_process_event(rdpSvcPlugin *plugin, RDP_EVENT *event)
{
	printf("xrdpvr: xrdpvr_process_event:\n");

	/* events comming from main freerdp window to plugin */
	/* send them back with svc_plugin_send_event */

	freerdp_event_free(event);
}

/*
 ******************************************************************************/
static void xrdpvr_process_terminate(rdpSvcPlugin *plugin)
{
	xrdpvrPlugin *xrdpvr = (xrdpvrPlugin *) plugin;

	printf("xrdpvr: xrdpvr_process_terminate:\n");

	if (xrdpvr == NULL)
	{
		return;
	}

	stream_free(xrdpvr->s);

	if (xrdpvr->audio_device)
	{
		(xrdpvr->audio_device->Free) (xrdpvr->audio_device);
	}

	xfree(plugin);
}

DEFINE_SVC_PLUGIN(xrdpvr, "xrdpvr",
                  CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)

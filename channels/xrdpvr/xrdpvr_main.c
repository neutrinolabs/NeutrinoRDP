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

#define DEBUG_XRDPVR(x...) //printf(x)

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

void xrdpvr_process_command(rdpSvcPlugin *plugin, STREAM *data_in);

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

	int bytes_to_process;     /* bytes  to process */
	int cmd_len;           /* # bytes required by current cmd */
	int i;

	if (xrdpvr == NULL)
	{
		DEBUG_XRDPVR("xrdpvr_process_receive: returning coz xrdpvr is NULL\n");
		stream_free(data_in);
		return;
	}

	if ((bytes_to_process = stream_get_size(data_in)) <= 0)
	{
		stream_free(data_in);
		return;
	}

	if (xrdpvr->got_partial_data)
	{
		/* handle pkt fragmentation */

		if (xrdpvr->bytes_needed < 0)
		{
			/* cannot compute cmd len bcoz xrdpvr->s contains */
			/* less than 4 bytes; copy required # of bytes to */
			/* xrdpvr->s so we have exactly 4 bytes in it     */
			i = 4 - (xrdpvr->s->p - xrdpvr->s->data);
			memcpy(xrdpvr->s->p, data_in->p, i);
			xrdpvr->s->p += i;
			data_in->p += i;
			bytes_to_process -= i;

			/* now we can read cmd len from xrdpvr->s and process */
			/* the data from data_in                              */
			stream_read_uint32(xrdpvr->s, cmd_len);
			xrdpvr->got_partial_data = 0;
			xrdpvr->bytes_needed = 0;
			goto label1;
		}
		else
		{
			if (bytes_to_process < xrdpvr->bytes_needed)
			{
				/* we still don't have enough data; save  */
				/* current data and wait for next pkt     */
				memcpy(xrdpvr->s->p, data_in->p, bytes_to_process);
				xrdpvr->s->p += bytes_to_process;
				xrdpvr->bytes_needed -= bytes_to_process;
				stream_free(data_in);
				return;
			}

			/* we have enough data to process cmd */
			memcpy(xrdpvr->s->p, data_in->p, xrdpvr->bytes_needed);
			xrdpvr->s->p = xrdpvr->s->data;
			bytes_to_process -= xrdpvr->bytes_needed;
			xrdpvr->bytes_needed = 0;
			xrdpvr->got_partial_data = 0;
			xrdpvr_process_command(plugin, xrdpvr->s);
		}
	}

	while (bytes_to_process > 0)
	{
		if (bytes_to_process < 4)
		{
			/* not enough data to determine cmd len */
			xrdpvr->bytes_needed = -1;
			xrdpvr->got_partial_data = 1;
			xrdpvr->s->p = xrdpvr->s->data;
			memcpy(xrdpvr->s->p, data_in->p, bytes_to_process);
			xrdpvr->s->p += bytes_to_process;
			stream_free(data_in);
			return;
		}

		/* get # bytes required by current cmd */
		stream_read_uint32(data_in, cmd_len);
		bytes_to_process -= 4;
label1:
		if (bytes_to_process >= cmd_len)
		{
			/* we have enough data to process this cmd */
			xrdpvr_process_command(plugin, data_in);
			bytes_to_process -= cmd_len;
		}
		else
		{
			/* we need more data to process this cmd */
			xrdpvr->bytes_needed = cmd_len - bytes_to_process;
			xrdpvr->got_partial_data = 1;

			/* save residual data */
			xrdpvr->s->p = xrdpvr->s->data;
			memcpy(xrdpvr->s->p, data_in->p, bytes_to_process);
			xrdpvr->s->p += bytes_to_process;
			stream_free(data_in);
			return;
		}
	}

	stream_free(data_in);
}

void xrdpvr_process_command(rdpSvcPlugin *plugin, STREAM *s)
{
	xrdpvrPlugin*   xrdpvr = (xrdpvrPlugin *) plugin;
	uint32          cmd;
	uint32          tmp;
	uint32		xpos = 0;
	uint32		ypos = 0;
	uint32          data_len;
	uint8*          decoded_data;
	uint32          uncompressed_size;
	int		width = 0;
	int		height = 0;
	int             rv;

	stream_read_uint32(s, cmd);

	switch (cmd)
	{
	case CMD_SEND_AUDIO_DATA:
		DEBUG_XRDPVR("###### got CMD_SEND_AUDIO_DATA\n");
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
		DEBUG_XRDPVR("###### got CMD_SEND_VIDEO_DATA\n");
		stream_read_uint32(s, tmp); /* stream id */
		stream_read_uint32(s, data_len);

		/* TODO check for return */
		process_video(g_psi, s->p, data_len);
		s->p += data_len;

		/* TODO do we need this: av_free_packet(&av_pkt); */
		break;

	case CMD_SET_GEOMETRY:
		DEBUG_XRDPVR("###### got CMD_SET_GEOMETRY\n");
		stream_read_uint32(s, tmp); /* stream id */
		stream_read_uint32(s, xpos);
		stream_read_uint32(s, ypos);
		stream_read_uint32(s, width);
		stream_read_uint32(s, height);
		set_geometry(g_psi, xpos, ypos, width, height);
		break;

	case CMD_SET_VIDEO_FORMAT:
		DEBUG_XRDPVR("###### got CMD_SET_VIDEO_FORMAT\n");
		stream_read_uint32(s, tmp); /* stream id */
		g_psi = init_player((void *) plugin, META_DATA_FILEAME);
		if (g_psi == NULL)
		{
			printf("init_player() failed\n");
		}

		break;

	case CMD_SET_AUDIO_FORMAT:
		DEBUG_XRDPVR("###### got CMD_SET_AUDIO_FORMAT\n");
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
		DEBUG_XRDPVR("###### got CMD_CREATE_META_DATA_FILE\n");
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
		DEBUG_XRDPVR("###### got CMD_CLOSE_META_DATA_FILE\n");
		close(g_meta_data_fd);
		break;

	case CMD_WRITE_META_DATA:
		DEBUG_XRDPVR("###### got CMD_WRITE_META_DATA\n");
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
		DEBUG_XRDPVR("###### got CMD_DEINIT_XRDPVR\n");
		stream_read_uint32(s, tmp); /* stream id */
		deinit_player(g_psi);
		break;

	default:
		// LK_TODO change to DEBUG_XRDPVR
		printf("### got unknown command 0x%x %d(.)\n", cmd, cmd);
		break;
	}

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
	plugin->s = stream_new(1024 * 1024 * 2);
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

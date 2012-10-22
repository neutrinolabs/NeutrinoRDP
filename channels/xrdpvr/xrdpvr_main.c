/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Xrdp video redirection channel
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
 * Copyright 2012 Vic Lee
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

#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/event.h>
#include <freerdp/plugins/tsmf.h>

#include "xrdpvr_common.h"
#include "xrdpvr_main.h"


struct xrdpvr_plugin
{
	rdpSvcPlugin	plugin;
	void			*decoder; /* reference to XrdpMpegDecoder */
	STREAM*			s;
	int				got_partial_data;
	int				bytes_needed;
};

#if 0
/*
 ******************************************************************************/
static void xrdpvr_process_interval(rdpSvcPlugin* plugin)
{
	printf("xrdpvr: xrdpvr_process_interval:\n");
}
#endif

/*
 ******************************************************************************/
static void xrdpvr_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	xrdpvrPlugin* 	xrdpvr = (xrdpvrPlugin*) plugin;
	STREAM*			s;
	int				rv;
	int				bytes_to_process;
	int				i;
	int error;
	uint32			cmd;
	uint32			data_len;
	uint32			tmp;
	XrdpMediaInfo	media_info;
	RDP_VIDEO_FRAME_EVENT* vevent;
	uint8* dd;
	uint32 ii, ww, hh, ff;

    //printf("xrdpvr_process_receive\n");
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
		/* previous pkt was fragmented and could not be processed; if we have */
		/* enough data, process it now, else defer processing till next pkt   */

		/* how much additional data did we get? */
		int data_in_len = data_in->size;

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
	}
	else
	{
		stream_read_uint32(data_in, data_len);
		if ((data_in->size - 4) < data_len)
		{
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
		xrdpvr->got_partial_data = 0;
		xrdpvr->bytes_needed = 0;
		s = data_in;
	}

	stream_read_uint32(s, cmd);
	switch (cmd)
	{
		case CMD_SEND_VIDEO_DATA:
			stream_read_uint32(s, tmp); /* stream id LK_TODO use this */
			stream_read_uint32(s, data_len);

			/* LK_TODO : s->p must be FF_INPUT_BUFFER_PADDING_SIZE padded */
#if 0
			rv = xrdpvr_decode((void *) xrdpvr->decoder, data_len, s->p, 0);
#else
			rv = xrdpvr_decode(xrdpvr->decoder, data_len, s->p, 1);

#endif
			s->p += data_len;
			if (rv != 0)
			{
				/* LK_TODO need to handle error */
				goto end;
			}

			dd = xrdpv_get_decoded_data(xrdpvr->decoder, &ii);
			if (dd == 0)
			{
				break;
			}
			error = xrdpvr_get_decoded_dimension(xrdpvr->decoder, &ww, &hh);
			if (error != 0)
			{
				break;
			}
			ff = xrdpvr_get_decoded_format(xrdpvr->decoder);
			if (ff == 0xffffffff)
			{
				break;
			}
			vevent = (RDP_VIDEO_FRAME_EVENT*)
					freerdp_event_new(RDP_EVENT_CLASS_TSMF,
							RDP_EVENT_TYPE_TSMF_VIDEO_FRAME,
							NULL, NULL);
			vevent->frame_data = dd;
			vevent->frame_size = ii;
			vevent->frame_pixfmt = ff;
			vevent->frame_width = ww;
			vevent->frame_height = hh;
			vevent->x = 0;
			vevent->y = 0;
			vevent->width = 1024;
			vevent->height = 768;
			vevent->num_visible_rects = 1;
			vevent->visible_rects = xmalloc(sizeof(RDP_RECT));
			vevent->visible_rects->x = 0;
			vevent->visible_rects->y = 0;
			vevent->visible_rects->width = 10;
			vevent->visible_rects->height = 10;
			error = svc_plugin_send_event(plugin, (RDP_EVENT*) vevent);
			if (error != 0)
			{
				freerdp_event_free((RDP_EVENT*) vevent);
			}
			break;

		case CMD_SET_VIDEO_FORMAT:
			stream_read_uint32(s, media_info.MajorType);
			stream_read_uint32(s, media_info.stream_id);
			stream_read_uint32(s, media_info.SubType);
			stream_read_uint32(s, media_info.FormatType);
			stream_read_uint32(s, media_info.width);
			stream_read_uint32(s, media_info.height);
			stream_read_uint32(s, media_info.bit_rate);
			stream_read_uint32(s, media_info.samples_per_sec_num);
			stream_read_uint32(s, media_info.samples_per_sec_den);
			stream_read_uint32(s, media_info.ExtraDataSize);
			if (media_info.ExtraDataSize)
			{
				/* LK_TODO where is this getting freed? */
				media_info.ExtraData = malloc(media_info.ExtraDataSize);
				stream_read(s, media_info.ExtraData, media_info.ExtraDataSize);
			}
			else
			{
				media_info.ExtraData = NULL;
			}

			printf("### got CMD_SET_VIDEO_FORMAT\n");
			printf("### MajorType=%d stream_id=%d SubType=0x%x FormatType=0x%x width=%d height=%d bit_rate=%d "
				   "samples_per_sec_num=%d samples_per_sec_den=%d ExtraDataSize=%d\n",
				   media_info.MajorType, media_info.stream_id, media_info.SubType, media_info.FormatType,
				   media_info.width, media_info.height, media_info.bit_rate, media_info.samples_per_sec_num,
				   media_info.samples_per_sec_den, media_info.ExtraDataSize);

			if (xrdpvr_set_format(xrdpvr->decoder, &media_info) != 0)
			{
				/* LK_TODO need to handle error */
				printf("### xrdpvr_set_format() failed\n");
				goto end;
			}
			printf("### xrdpvr_set_format() passed\n");
			printf("\n\n................................................................\n\n");
			break;

		case CMD_SET_AUDIO_FORMAT:
			printf("### LK_TODO: got CMD_SET_AUDIO_FORMAT\n");
			break;

		default:
			printf("### got unknown command 0x%x %d(.)\n", cmd, cmd);
			break;
	}

end:

	if (s == xrdpvr->s)
	{
		/* we just finished processing a fragmented pkt; */
		/* did we get part of the next command as well?  */

		i = stream_get_length(xrdpvr->s);
		if (bytes_to_process > i)
		{
			/* yes we did - copy this to data_in and start all over */
			printf("### xrdpvr_process_receive: got part of next command\n");
			memcpy(data_in->data, s->p, bytes_to_process - i);
			data_in->p = data_in->data;
			xrdpvr->got_partial_data = 0;
			xrdpvr->bytes_needed = 0;
			bytes_to_process = 0;
			goto start;
		}
	}

	stream_free(data_in);

// LK_TODO
#if 0
	if (bytes > 0)
	{
		data_out = stream_new(bytes);
		stream_copy(data_out, data_in, bytes);

		/*
		 * svc_plugin_send takes ownership of data_out,
		 * that is why we do not free it
		 */

		bytes = stream_get_length(data_in);
		printf("xrdpvr: xrdpvr_process_receive: sending bytes %d\n", bytes);

		svc_plugin_send(plugin, data_out);
	}
#endif
}

/*
 ******************************************************************************/
static void xrdpvr_process_connect(rdpSvcPlugin* plugin_p)
{
	xrdpvrPlugin* plugin = (xrdpvrPlugin*) plugin_p;

	if (plugin == NULL)
		return;

	/* if you want a call from channel thread once is a while do this */
// LK_TODO
#if 0
	plugin_p->interval_ms = 10000000; // LK_TODO
	plugin_p->interval_callback = xrdpvr_process_interval;
#endif

	/* setup stream */
	plugin->s = stream_new(1024 * 1024);
	plugin->got_partial_data = 0;
	plugin->bytes_needed = 0;

	/* setup ffmpeg */
	plugin->decoder = xrdpvr_init();
}

/*
 ******************************************************************************/
static void xrdpvr_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	printf("xrdpvr: xrdpvr_process_event:\n");

	/* events comming from main freerdp window to plugin */
	/* send them back with svc_plugin_send_event */

	freerdp_event_free(event);
}

/*
 ******************************************************************************/
static void xrdpvr_process_terminate(rdpSvcPlugin* plugin)
{
	xrdpvrPlugin* xrdpvr = (xrdpvrPlugin*) plugin;

	printf("xrdpvr: xrdpvr_process_terminate:\n");

	if (xrdpvr == NULL)
		return;

	stream_free(xrdpvr->s);
	xfree(plugin);
}

DEFINE_SVC_PLUGIN(xrdpvr, "xrdpvr",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)

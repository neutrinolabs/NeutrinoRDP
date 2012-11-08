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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <libavformat/avformat.h>

//#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/event.h>
#include <freerdp/plugins/tsmf.h>

#include "xrdpvr_player.h"

typedef struct player_state_info
{
	AVFormatContext *format_ctx;
	AVCodecContext  *audio_codec_ctx;
	AVCodecContext  *video_codec_ctx;
	AVCodec         *audio_codec;
	AVCodec         *video_codec;
	AVFrame         *audio_frame;
	AVFrame         *video_frame;

	int              audio_stream_index;
	int              video_stream_index;
	uint8_t         *video_decoded_data;
	uint32_t         video_decoded_size;
	uint8_t         *audio_decoded_data;
	uint32_t         audio_decoded_size;
	uint32_t         decoded_size_max;
	void            *plugin;
	int              player_inited;

} PLAYER_STATE_INFO;

/* forward declarations local to this file */
static int play_video(PLAYER_STATE_INFO *psi, AVPacket *av_pkt);
static int play_audio(PLAYER_STATE_INFO *psi, AVPacket *av_pkt);
static uint8_t *get_decoded_video_data(PLAYER_STATE_INFO *psi, uint32_t *size);
static int get_decoded_video_dimension(PLAYER_STATE_INFO *psi, uint32_t *width, uint32_t *height);
static uint32_t get_decoded_video_format(PLAYER_STATE_INFO *psi);
static int display_picture(PLAYER_STATE_INFO *psi);
static int does_file_exist(char *filename);

/**
 ******************************************************************************/
void *
init_player(void *plugin, char *filename)
{
	PLAYER_STATE_INFO *psi;

	int video_index = -1;
	int audio_index = -1;
	int i;

	/* to hold player state information */
	psi = (PLAYER_STATE_INFO *) calloc(1, sizeof(PLAYER_STATE_INFO));

	if (psi == NULL)
	{
		return NULL;
	}

	psi->plugin = plugin;

	if ((g_meta_data_fd < 0) || (does_file_exist(filename) == 0))
	{
		return NULL;
	}

	/* register all available fileformats and codecs */
	av_register_all();

	/* open media file - this will read just the header */
	if (avformat_open_input(&psi->format_ctx, filename, NULL, NULL))
	{
		DEBUG_WARN("xrdp_player.c:init_player: ERROR opening %s\n", filename);
		goto bailout1;
	}

	/* now get the real stream info */
	if (avformat_find_stream_info(psi->format_ctx, NULL) < 0)
	{
		DEBUG_WARN("xrdp_player.c:init_player: ERROR reading stream info\n");
		goto bailout1;
	}

#if 0
	/* display basic media info */
	av_dump_format(psi->format_ctx, 0, filename, 0);
#endif

	/* TODO: provide support for selecting audio/video stream when multiple */
	/* streams are present, such as multilingual audio or widescreen video  */

	/* find first audio / video stream */
	for (i = 0; i < psi->format_ctx->nb_streams; i++)
	{
		if (psi->format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
		        video_index < 0)
		{
			video_index = i;
		}

		if (psi->format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
		        audio_index < 0)
		{
			audio_index = i;
		}
	}

	if ((audio_index < 0) && (video_index < 0))
	{
		/* file does not contain audio or video */
		DEBUG_WARN("xrdp_player.c:init_player: "
		           "no audio/video stream found in %s\n", filename);
		goto bailout2;
	}

	psi->audio_stream_index = audio_index;
	psi->video_stream_index = video_index;

	/* save codex contexts for both streams */
	psi->audio_codec_ctx = psi->format_ctx->streams[audio_index]->codec;
	psi->video_codec_ctx = psi->format_ctx->streams[video_index]->codec;

	/* find decoder for audio stream */
	psi->audio_codec = avcodec_find_decoder(psi->audio_codec_ctx->codec_id);

	if (psi->audio_codec == NULL)
	{
		DEBUG_WARN("xrdp_player.c:init_player: "
		           "ERROR: audio codec not supported\n");
		goto bailout2;
	}

	/* find decoder for video stream */
	psi->video_codec = avcodec_find_decoder(psi->video_codec_ctx->codec_id);

	if (psi->video_codec == NULL)
	{
		DEBUG_WARN("xrdp_player.c:init_player: "
		           "ERROR: video codec not supported\n");
		goto bailout2;
	}

	/* open decoder for audio stream */
	if (avcodec_open2(psi->audio_codec_ctx, psi->audio_codec, NULL) < 0)
	{
		DEBUG_WARN("xrdp_player.c:init_player: "
		           "ERROR: could not open audio decoder\n");
		goto bailout2;
	}

	/* open decoder for video stream */
	if (avcodec_open2(psi->video_codec_ctx, psi->video_codec, NULL) < 0)
	{
		DEBUG_WARN("xrdp_player.c:init_player: "
		           "ERROR: could not open video decoder\n");
		goto bailout2;
	}

	/* allocate frames */
	psi->audio_frame = avcodec_alloc_frame();
	psi->video_frame = avcodec_alloc_frame();

	psi->player_inited = 1;
	return psi;

bailout2:
	avformat_close_input(&psi->format_ctx);

bailout1:
	free(psi);
	return NULL;
}

/**
 ******************************************************************************/
void
deinit_player(void *vp)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return;
	}

	av_free(psi->audio_frame);
	av_free(psi->video_frame);
	avcodec_close(psi->audio_codec_ctx);
	avcodec_close(psi->video_codec_ctx);
	avformat_close_input(&psi->format_ctx);
	free(psi);
}

int
process_video(void *vp, uint8 *data, int data_len)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;
	AVPacket av_pkt;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

	av_init_packet(&av_pkt);
	av_pkt.data = data;
	av_pkt.size = data_len;
	play_video(psi, &av_pkt);
	return 0;
}

int
process_audio(void *vp, uint8 *data, int data_len)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;
	AVPacket av_pkt;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

	av_init_packet(&av_pkt);
	av_pkt.data = data;
	av_pkt.size = data_len;

	if (psi->audio_decoded_data)
	{
		free(psi->audio_decoded_data);
		psi->audio_decoded_data = NULL;
	}

	psi->audio_decoded_size = 0;

	play_audio(psi, &av_pkt);
	return 0;
}

uint8_t *
get_decoded_audio_data(void *vp, uint32_t *size)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;
	uint8_t *buf;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return NULL;
	}

	buf = psi->audio_decoded_data;
	*size = psi->audio_decoded_size;

	psi->audio_decoded_data = NULL;
	psi->audio_decoded_size = 0;

	return buf;
}

/*
 * according to channels/rdpsnd/alsa/rdpsnd_alsa.c, alsa supports only
 * 8 and 16 bits per sample
 ******************************************************************************/
void
get_audio_config(void *vp, int *samp_per_sec, int *num_channels, int *bits_per_samp)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return;
	}

	*samp_per_sec = psi->audio_codec_ctx->sample_rate;
	*num_channels = psi->audio_codec_ctx->channels;

	switch (psi->audio_codec_ctx->sample_fmt)
	{
		case AV_SAMPLE_FMT_U8:
			*bits_per_samp = 8;
			break;

		default:
			*bits_per_samp = 16;
			break;
	}
}

/*******************************************************************************
                           functions local to this file
*******************************************************************************/

/**
 ******************************************************************************/
static int
play_video(PLAYER_STATE_INFO *psi, AVPacket *av_pkt)
{
	AVFrame *frame;
	int      len;
	int      got_frame;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

	/* TODO need to handle older versions - see Vic's code */
	len = avcodec_decode_video2(psi->video_codec_ctx, psi->video_frame, &got_frame,
	                            av_pkt);

	if (len < 0)
	{
		DEBUG_WARN("xrdp_player.c:play_video: frame decode error\n");
		return -1;
	}

	if (!got_frame)
	{
		DEBUG_WARN("xrdp_player.c:play_video:  nothing decoded!\n");
		return -1;
	}

	if (len != av_pkt->size)
	{
		DEBUG_WARN("decoded only %d/%d bytes\n", len, av_pkt->size);
	}

	psi->video_decoded_size = avpicture_get_size(psi->video_codec_ctx->pix_fmt,
	                          psi->video_codec_ctx->width,
	                          psi->video_codec_ctx->height);

	/* TODO where is this memory released? */
	psi->video_decoded_data = xzalloc(psi->video_decoded_size);

	frame = avcodec_alloc_frame();

	avpicture_fill((AVPicture *) frame, psi->video_decoded_data,
	               psi->video_codec_ctx->pix_fmt,
	               psi->video_codec_ctx->width,
	               psi->video_codec_ctx->height);

	av_picture_copy((AVPicture *) frame, (AVPicture *) psi->video_frame,
	                psi->video_codec_ctx->pix_fmt,
	                psi->video_codec_ctx->width,
	                psi->video_codec_ctx->height);

	av_free(frame);
	display_picture(psi);

	return 0;
}

static int
display_picture(PLAYER_STATE_INFO *psi)
{
	RDP_VIDEO_FRAME_EVENT *vevent;

	uint8_t     *decoded_data;
	uint32_t     decoded_len;
	uint32_t     width;
	uint32_t     height;
	uint32_t     video_format;

	if ((decoded_data = get_decoded_video_data(psi, &decoded_len)) == 0)
	{
		return -1;
	}

	if (get_decoded_video_dimension(psi, &width, &height) != 0)
	{
		return -1;
	}

	if ((video_format = get_decoded_video_format(psi)) < 0)
	{
		return -1;
	}

	vevent = (RDP_VIDEO_FRAME_EVENT *) freerdp_event_new(RDP_EVENT_CLASS_TSMF,
	         RDP_EVENT_TYPE_TSMF_VIDEO_FRAME,
	         NULL, NULL);

	vevent->frame_data = decoded_data;
	vevent->frame_size = decoded_len;
	vevent->frame_pixfmt = video_format;
	vevent->frame_width = width;
	vevent->frame_height = height;

	/* TODO these hard coded values need to change */
	vevent->x = 0;
	vevent->y = 0;
	vevent->width = psi->video_codec_ctx->width;
	vevent->height = psi->video_codec_ctx->height;
	vevent->num_visible_rects = 1;
	vevent->visible_rects = xmalloc(sizeof(RDP_RECT));
	vevent->visible_rects->x = 0;
	vevent->visible_rects->y = 0;
	vevent->visible_rects->width = 10;
	vevent->visible_rects->height = 10;

	if (svc_plugin_send_event(psi->plugin, (RDP_EVENT *) vevent) != 0)
	{
		freerdp_event_free((RDP_EVENT *) vevent);
	}

	return 0;
}

static uint8_t *
get_decoded_video_data(PLAYER_STATE_INFO *psi, uint32_t *size)
{
	uint8_t *buf;

	buf = psi->video_decoded_data;
	*size = psi->video_decoded_size;

	psi->video_decoded_data = NULL;
	psi->video_decoded_size = 0;

	return buf;
}

static int
get_decoded_video_dimension(PLAYER_STATE_INFO *psi, uint32_t *width, uint32_t *height)
{
	if ((psi->video_codec_ctx->width > 0) && (psi->video_codec_ctx->height > 0))
	{
		*width = psi->video_codec_ctx->width;
		*height = psi->video_codec_ctx->height;
		return 0;
	}

	return -1;
}

/*
 ******************************************************************************/
static uint32_t
get_decoded_video_format(PLAYER_STATE_INFO *psi)
{
	switch (psi->video_codec_ctx->pix_fmt)
	{
		case PIX_FMT_YUV420P:
			return RDP_PIXFMT_I420;

		default:
			DEBUG_WARN("unsupported pixel format %u\n",
			           psi->video_codec_ctx->pix_fmt);
			return (uint32) - 1;
	}
}

/**
 ******************************************************************************/
static int
play_audio(PLAYER_STATE_INFO *psi, AVPacket *av_pkt)
{
	int         len;
	int         frame_size;
	uint32_t    src_size;
	int         dst_offset;

	const uint8_t *src;
	uint8_t       *dst;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

	if (psi->decoded_size_max == 0)
	{
		psi->decoded_size_max = AVCODEC_MAX_AUDIO_FRAME_SIZE + 16;
	}

	/* TODO where does this memory get released? */
	psi->audio_decoded_data = xzalloc(psi->decoded_size_max);

	/* align the memory for SSE2 needs */
	dst = (uint8_t *) (((uintptr_t) psi->audio_decoded_data + 15) & ~ 0x0F);
	dst_offset = dst - psi->audio_decoded_data;
	src = av_pkt->data;
	src_size = av_pkt->size;

	while (src_size > 0)
	{
		/* Ensure enough space for decoding */
		if (psi->decoded_size_max - psi->audio_decoded_size < AVCODEC_MAX_AUDIO_FRAME_SIZE)
		{
			psi->decoded_size_max = psi->decoded_size_max * 2 + 16;
			psi->audio_decoded_data = realloc(psi->audio_decoded_data, psi->decoded_size_max);
			dst = (uint8_t *) (((uintptr_t)psi->audio_decoded_data + 15) & ~ 0x0F);

			if (dst - psi->audio_decoded_data != dst_offset)
			{
				/* re-align the memory if the alignment has changed after realloc */
				memmove(dst, psi->audio_decoded_data + dst_offset, psi->audio_decoded_size);
				dst_offset = dst - psi->audio_decoded_data;
			}

			dst += psi->audio_decoded_size;
		}

		frame_size = psi->decoded_size_max - psi->audio_decoded_size;
#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
		len = avcodec_decode_audio2(psi->codec_context,
		                            (int16_t *) dst, &frame_size, src, src_size);
#else
		{
			AVFrame *decoded_frame = avcodec_alloc_frame();
			int got_frame = 0;
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = (uint8_t *) src;
			pkt.size = src_size;

			len = avcodec_decode_audio4(psi->audio_codec_ctx,
						    decoded_frame,
						    &got_frame, &pkt);

			if (len >= 0 && got_frame)
			{
				frame_size = av_samples_get_buffer_size(NULL,
									psi->audio_codec_ctx->channels,
				                                        decoded_frame->nb_samples,
									psi->audio_codec_ctx->sample_fmt,
									1);

				memcpy(dst, decoded_frame->data[0], frame_size);
			}

			av_free(decoded_frame);
		}
#endif

		if (len <= 0 || frame_size <= 0)
		{
			DEBUG_WARN("error decoding");
			break;
		}

		src += len;
		src_size -= len;
		psi->audio_decoded_size += frame_size;
		dst += frame_size;
	}

	if (psi->audio_decoded_size == 0)
	{
		free(psi->audio_decoded_data);
		psi->audio_decoded_data = NULL;
	}
	else if (dst_offset)
	{
		/* move the aligned decoded data to original place */
		memmove(psi->audio_decoded_data, psi->audio_decoded_data + dst_offset, psi->audio_decoded_size);
	}

	return 0;
}

/**
 * determine if specified file exists
 *
 * @param  filename the filename to check for existance
 *
 * @return 1 if file exists, 0 otherwise
 ******************************************************************************/
static int
does_file_exist(char *filename)
{
	if (access(filename, F_OK) == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

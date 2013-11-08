/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Xrdp video redirection channel
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

/*
centos 5.8
package qffmpeg-devel
/usr/include/qffmpeg/libqavcodec
#define LIBQAVCODEC_VERSION_MAJOR 51
#define LIBQAVCODEC_VERSION_MINOR 71
#define LIBQAVCODEC_VERSION_MICRO  0

debian 6
#define LIBAVCODEC_VERSION_MAJOR 52
#define LIBAVCODEC_VERSION_MINOR 20
#define LIBAVCODEC_VERSION_MICRO  1

ubuntu 10.04
#define LIBAVCODEC_VERSION_MAJOR 52
#define LIBAVCODEC_VERSION_MINOR 20
#define LIBAVCODEC_VERSION_MICRO  1

ubuntu 10.10
#define LIBAVCODEC_VERSION_MAJOR 52
#define LIBAVCODEC_VERSION_MINOR 72
#define LIBAVCODEC_VERSION_MICRO  2

ubuntu 11.04
#define LIBAVCODEC_VERSION_MAJOR 52
#define LIBAVCODEC_VERSION_MINOR 72
#define LIBAVCODEC_VERSION_MICRO  2

ubuntu 11.11
#define LIBAVCODEC_VERSION_MAJOR 53
#define LIBAVCODEC_VERSION_MINOR 34
#define LIBAVCODEC_VERSION_MICRO  0

mint 13
#define LIBAVCODEC_VERSION_MAJOR 53
#define LIBAVCODEC_VERSION_MINOR 35
#define LIBAVCODEC_VERSION_MICRO  0

*/

#if LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR == 20
#define DISTRO_DEBIAN6
#endif

#if LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR == 72
#define DISTRO_UBUNTU1104
#endif

#if LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_MINOR == 34
#define DISTRO_UBUNTU1111
#endif

#if LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_MINOR == 35
#define DISTRO_UBUNTU1204
#endif

#if !defined(DISTRO_DEBIAN6) && !defined(DISTRO_UBUNTU1104) && \
    !defined(DISTRO_UBUNTU1111) && !defined(DISTRO_UBUNTU1204)
#warning unsupported distro
#endif

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
//#include "multiformatdecoder.h"

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

	int              audio_prepared;
	int              video_prepared;

	int              xpos;
	int              ypos;
	int              width;
	int              height;
	int              volume;

	//void*		 hw_ctx;
	void*            dec;

} PLAYER_STATE_INFO;

/* forward declarations local to this file */
static int play_video(PLAYER_STATE_INFO *psi, struct AVPacket *av_pkt);
static int play_audio(PLAYER_STATE_INFO *psi, AVPacket *av_pkt);
static uint8_t *get_decoded_video_data(PLAYER_STATE_INFO *psi, uint32_t *size);
static int get_decoded_video_dimension(PLAYER_STATE_INFO *psi, uint32_t *width, uint32_t *height);
static uint32_t get_decoded_video_format(PLAYER_STATE_INFO *psi);
static int display_picture(PLAYER_STATE_INFO *psi);

#if defined(DISTRO_UBUNTU1204) || defined(DISTRO_UBUNTU1111)
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#endif

void* init_context(int codec_id);
void destroy_cts(void*ctx);
int decode_video(void*ctx, int* got_frame, unsigned char* data, int size);
int get_output_frame(void*ctx, unsigned char* data, int size);
int get_decoded_size(void*ctx);
void get_decodec_dimentions(void*ctx,int* width,int* height);

/**
 ******************************************************************************/
void* init_player(void* plugin, char* filename)
{
	PLAYER_STATE_INFO *psi;

	printf("init_player:\n");
	psi = (PLAYER_STATE_INFO *) calloc(1, sizeof(PLAYER_STATE_INFO));
	if (psi == NULL)
	{
		return NULL;
	}

	psi->plugin = plugin;

	/* register all available fileformats and codecs */
	av_register_all();

	psi->audio_codec_ctx = avcodec_alloc_context();
	psi->audio_codec = avcodec_find_decoder(CODEC_ID_AAC);

	psi->video_codec_ctx = avcodec_alloc_context();
	psi->video_codec = avcodec_find_decoder(CODEC_ID_H264);

	psi->audio_frame = avcodec_alloc_frame();
	psi->video_frame = avcodec_alloc_frame();

	psi->player_inited = 1;

	return psi;
}

/**
 ******************************************************************************/
void
deinit_player(void *vp)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	printf("deinit_player:\n");
	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return;
	}

	av_free(psi->audio_frame);
	av_free(psi->video_frame);

	if (psi->audio_prepared)
	{
		avcodec_close(psi->audio_codec_ctx);
	}
	av_free(psi->audio_codec_ctx);
	if (psi->video_prepared)
	{
		avcodec_close(psi->video_codec_ctx);
	}
	av_free(psi->video_codec_ctx);

	free(psi);
}

/**
 ******************************************************************************/
int
process_video(void *vp, uint8 *data, int data_len)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;
	AVPacket av_pkt;

	//printf("process_video:\n");
	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

#if 1
	av_init_packet(&av_pkt);
	av_pkt.data = data;
	av_pkt.size = data_len;
	play_video(psi, &av_pkt);
#endif

	return 0;
}

/**
 ******************************************************************************/
int
process_audio(void *vp, uint8 *data, int data_len)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;
	AVPacket av_pkt;

	//printf("process_audio:\n");
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

/**
 ******************************************************************************/
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
		//case AV_SAMPLE_FMT_U8:
		case SAMPLE_FMT_U8:
			*bits_per_samp = 8;
			break;

		default:
			*bits_per_samp = 16;
			break;
	}
}

/**
 ******************************************************************************/
void
set_audio_config(void* vp, char* extradata, int extradata_size,
		int sample_rate, int bit_rate, int channels, int block_align)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	printf("set_audio_config:\n");
	psi->audio_codec_ctx->extradata_size = extradata_size + 8;
	psi->audio_codec_ctx->extradata =
			xzalloc(psi->audio_codec_ctx->extradata_size);
	memcpy(psi->audio_codec_ctx->extradata, extradata, extradata_size);
	psi->audio_codec_ctx->sample_rate = sample_rate;
	psi->audio_codec_ctx->bit_rate = bit_rate;
	psi->audio_codec_ctx->channels = channels;
	psi->audio_codec_ctx->block_align = block_align;
#ifdef AV_CPU_FLAG_SSE2
	psi->audio_codec_ctx->dsp_mask = AV_CPU_FLAG_SSE2 | AV_CPU_FLAG_MMX2;
#else
#if LIBAVCODEC_VERSION_MAJOR < 53
	psi->audio_codec_ctx->dsp_mask = FF_MM_SSE2 | FF_MM_MMXEXT;
#else
	psi->audio_codec_ctx->dsp_mask = FF_MM_SSE2 | FF_MM_MMX2;
#endif
#endif
	if (psi->audio_codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		psi->audio_codec_ctx->flags |= CODEC_FLAG_TRUNCATED;
	}
	if (avcodec_open(psi->audio_codec_ctx, psi->audio_codec) < 0)
	{
		printf("init_player: audio avcodec_open failed\n");
	}
	else
	{
		psi->audio_prepared = 1;
	}
}

/**
 ******************************************************************************/
void
set_video_config(void *vp)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	printf("set_video_config:\n");
	if (avcodec_open(psi->video_codec_ctx, psi->video_codec) < 0)
	{
		printf("init_player: video avcodec_open failed\n");
	}
	else
	{
		psi->video_prepared = 1;
	}
}

/**
 ******************************************************************************/
void
set_geometry(void *vp, int xpos, int ypos, int width, int height)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	printf("set_geometry: x=%d y=%d with=%d height=%d\n", xpos, ypos, width, height);
	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return;
	}

	psi->xpos = xpos;
	psi->ypos = ypos;
	psi->width = width;
	psi->height = height;
}

#define SAVE_VIDEO 0

#if SAVE_VIDEO
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/**
 ******************************************************************************/
static int n_save_data(const uint8* data, uint32 data_size, int width, int height)
{
    int fd;
    struct _header
    {
        char tag[4];
        int width;
        int height;
        int bytes_follow;
    } header;

    fd = open("video.bin", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    lseek(fd, 0, SEEK_END);
    header.tag[0] = 'B';
    header.tag[1] = 'E';
    header.tag[2] = 'E';
    header.tag[3] = 'F';
    header.width = width;
    header.height = height;
    header.bytes_follow = data_size;
    if (write(fd, &header, 16) != 16)
    {
        printf("save_data: write failed\n");
    }

    if (write(fd, data, data_size) != data_size)
    {
        printf("save_data: write failed\n");
    }
    close(fd);
    return 0;
}
#endif

/*******************************************************************************
                           functions local to this file
*******************************************************************************/

/**
 ******************************************************************************/
static int
//play_video(PLAYER_STATE_INFO *psi, AVPacket *av_pkt)
play_video(PLAYER_STATE_INFO *psi, struct AVPacket *av_pkt)
{
	AVFrame *frame;
	int      len = -1;
	int      got_frame;

	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return -1;
	}

#if SAVE_VIDEO
	n_save_data(av_pkt->data, av_pkt->size,
			psi->video_codec_ctx->width,
			psi->video_codec_ctx->height);
#endif

	psi->video_codec_ctx->width = 720;
	psi->video_codec_ctx->height = 404;
	psi->video_codec_ctx->pix_fmt = PIX_FMT_YUV420P;

#ifdef DISTRO_DEBIAN6
	len = avcodec_decode_video(psi->video_codec_ctx, psi->video_frame, &got_frame, av_pkt->data, av_pkt->size);
#else
	len = avcodec_decode_video2(psi->video_codec_ctx, psi->video_frame, &got_frame, av_pkt);
#endif

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
			psi->video_codec_ctx->width, psi->video_codec_ctx->height);

	/* TODO where is this memory released? */
	psi->video_decoded_data = xzalloc(psi->video_decoded_size);
	frame = avcodec_alloc_frame();
	avpicture_fill((AVPicture *) frame, psi->video_decoded_data,
			psi->video_codec_ctx->pix_fmt, psi->video_codec_ctx->width,
			psi->video_codec_ctx->height);
	av_picture_copy((AVPicture *) frame, (AVPicture *) psi->video_frame,
			psi->video_codec_ctx->pix_fmt, psi->video_codec_ctx->width,
			psi->video_codec_ctx->height);
	av_free(frame);
	display_picture(psi);
	return 0;
}

/**
 ******************************************************************************/
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
		printf("display_picture: get_decoded_video_data failed\n");
		return -1;
	}

	if (get_decoded_video_dimension(psi, &width, &height) != 0)
	{
		printf("display_picture: get_decoded_video_dimension failed\n");
		return -1;
	}

	if ((video_format = get_decoded_video_format(psi)) < 0)
	{
		printf("display_picture: get_decoded_video_format failed\n");
		return -1;
	}

	vevent = (RDP_VIDEO_FRAME_EVENT *) freerdp_event_new(RDP_EVENT_CLASS_TSMF,
			RDP_EVENT_TYPE_TSMF_VIDEO_FRAME, NULL, NULL);

	vevent->frame_data = decoded_data;
	vevent->frame_size = decoded_len;
	vevent->frame_pixfmt = video_format;
	vevent->frame_width = width;
	vevent->frame_height = height;

	//printf("display: x=%d y=%d with=%d height=%d\n", psi->xpos, psi->ypos, psi->width, psi->height);

	vevent->x = psi->xpos;
	vevent->y = psi->ypos;
#if 0
	vevent->width = psi->video_codec_ctx->width;
	vevent->height = psi->video_codec_ctx->height;
#else
	vevent->width = psi->width;
	vevent->height = psi->height;
#endif
	vevent->num_visible_rects = 1;
	vevent->visible_rects = xmalloc(sizeof(RDP_RECT));
	vevent->visible_rects->x = 0;
	vevent->visible_rects->y = 0;
	vevent->visible_rects->width = vevent->width;
	vevent->visible_rects->height = vevent->height;

	if (svc_plugin_send_event(psi->plugin, (RDP_EVENT *) vevent) != 0)
	{
		freerdp_event_free((RDP_EVENT *) vevent);
	}

	return 0;
}

/**
 ******************************************************************************/
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

/**
 ******************************************************************************/
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
			DEBUG_WARN("unsupported pixel format %u\n", psi->video_codec_ctx->pix_fmt);
			return (uint32) -1;
	}
}

/**
 ******************************************************************************/
static int
play_audio(PLAYER_STATE_INFO *psi, AVPacket *av_pkt)
{
	int         len = 0;
	int         frame_size;
	uint32_t    src_size;
	int         dst_offset;

	const uint8_t *src;
	uint8_t       *dst;

	//printf("play_audio: %d\n", av_pkt->size);
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
#ifdef DISTRO_DEBIAN6
		len = avcodec_decode_audio2(psi->audio_codec_ctx, (int16_t *) dst, &frame_size, src, src_size);
#endif
#ifdef DISTRO_UBUNTU1104
		if (1)
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = (uint8_t *) src;
			pkt.size = src_size;
			len = avcodec_decode_audio3(psi->audio_codec_ctx, (int16_t *) dst, &frame_size, &pkt);
		}
#endif
#if defined(DISTRO_UBUNTU1204) || defined(DISTRO_UBUNTU1111)
		if (1)
		{
			AVFrame *decoded_frame = avcodec_alloc_frame();
			int got_frame = 0;
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = (uint8_t *) src;
			pkt.size = src_size;

			len = avcodec_decode_audio4(psi->audio_codec_ctx, decoded_frame, &got_frame, &pkt);

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

/*****************************************************************************/
void
set_volume(void *vp, int volume)
{
	PLAYER_STATE_INFO *psi = (PLAYER_STATE_INFO *) vp;

	printf("set_volume: vol=%d\n", volume);
	if ((psi == NULL) || (!psi->player_inited))
	{
		DEBUG_WARN("xrdpvr player is NULL or not inited");
		return;
	}
	psi->volume = volume;
}

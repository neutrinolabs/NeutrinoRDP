/**
 * avcodec / ffmpeg calls
 *
 * Copyright 2015-2016 Jay Sorg <jay.sorg@gmail.com>
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

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

#include <freerdp/utils/nrdp_avcodec.h>

#ifndef LIBAVCODEC_VERSION_MAJOR
#warning LIBAVCODEC_VERSION_MAJOR not defined
#endif

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
        printf _args ; \
        printf("\n"); \
    } \
  } \
  while (0)

// LIBAVCODEC_VERSION_MAJOR 52 LIBAVCODEC_VERSION_MINOR 20 ubuntu 10.04
// LIBAVCODEC_VERSION_MAJOR 52 LIBAVCODEC_VERSION_MINOR 20 debian 6
// LIBAVCODEC_VERSION_MAJOR 53 LIBAVCODEC_VERSION_MINOR 35 ubuntu 12.04
// LIBAVCODEC_VERSION_MAJOR 53 LIBAVCODEC_VERSION_MINOR 35 debian 7
// LIBAVCODEC_VERSION_MAJOR 54 LIBAVCODEC_VERSION_MINOR 35 ubuntu 14.04
// LIBAVCODEC_VERSION_MAJOR 56 LIBAVCODEC_VERSION_MINOR 1  debian 8
// LIBAVCODEC_VERSION_MAJOR 56 LIBAVCODEC_VERSION_MINOR 60 ubuntu 16.04
// LIBAVCODEC_VERSION_MAJOR 56 LIBAVCODEC_VERSION_MINOR 60 FreeBSD(PCBSD10.1.2-05-22-2015)

//example
//#if (LIBAVCODEC_VERSION_INT <= ((51<<16) + (28<<8) + 0))
//              len = avcodec_decode_audio(cc, (int16_t *)output->buffer, &frame_size,
//                              input->curr_pkt_buf, input->curr_pkt_size);
//#else
//               /* The change to avcodec_decode_audio3 occurred between
//                * 52.25.0 and 52.26.0 */
//#elif (LIBAVCODEC_VERSION_INT <= ((52<<16) + (25<<8) + 0))
//              len = avcodec_decode_audio2(cc, (int16_t *) output->buffer, &frame_size,
//                              input->curr_pkt_buf, input->curr_pkt_size);
//#else
//               AVPacket avpkt;
//               av_init_packet(&avpkt);
//               avpkt.data = input->curr_pkt_buf;
//               avpkt.size = input->curr_pkt_size;
//               len = avcodec_decode_audio3(cc, (int16_t *) output->buffer, &frame_size, &avpkt);
//               av_free_packet(&avpkt);

#if LIBAVCODEC_VERSION_MAJOR > 55
#define CODEC_ID_AC3                AV_CODEC_ID_AC3
#define CODEC_ID_AAC                AV_CODEC_ID_AAC
#define CODEC_ID_MPEG2VIDEO         AV_CODEC_ID_MPEG2VIDEO
#define AVCODEC_ALLOC_FRAME         av_frame_alloc
#define AVCODEC_FREE_FRAME(_frame)  av_frame_free(_frame);
#else
#define AVCODEC_ALLOC_FRAME         avcodec_alloc_frame
#define AVCODEC_FREE_FRAME(_frame)  av_free(*(_frame));
#endif

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
#define AVCODEC_ALLOC_CONTEXT avcodec_alloc_context()
#else
#define AVCODEC_ALLOC_CONTEXT avcodec_alloc_context3(NULL)
#endif

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
#define AVCODEC_OPEN(_context, _codec) avcodec_open(_context, _codec)
#else
#define AVCODEC_OPEN(_context, _codec) avcodec_open2(_context, _codec, NULL)
#endif

struct avcodec_audio
{
    AVCodecContext* codec_context;
    AVCodec* codec;
    AVFrame* frame;
    short* dst;
    int dst_bytes;
    int pad0;
};

struct avcodec_video
{
    AVCodecContext* codec_context;
    AVCodec* codec;
    AVFrame* frame;
};

/*****************************************************************************/
int
nrdp_avcodec_init(void)
{
    avcodec_register_all();
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_create(void** obj, int codec_id)
{
    struct avcodec_audio* self;
    int error;
    int avcodec_id;

    if (obj == NULL)
    {
        return 1;
    }
    avcodec_id = 0;
    switch (codec_id)
    {
        case AUDIO_CODEC_ID_AC3:
            avcodec_id = CODEC_ID_AC3;
            break;
        case AUDIO_CODEC_ID_AAC:
            avcodec_id = CODEC_ID_AAC;
            //avcodec_id = CODEC_ID_AAC_LATM;
            break;
        default:
            return 6;
    }
    self = (struct avcodec_audio*)malloc(sizeof(struct avcodec_audio));
    if (self == NULL)
    {
        return 2;
    }
    memset(self, 0, sizeof(struct avcodec_audio));
    self->codec_context = AVCODEC_ALLOC_CONTEXT;
    if (self->codec_context == NULL)
    {
        free(self);
        return 3;
    }
    self->codec = avcodec_find_decoder(avcodec_id);
    if (self->codec == NULL)
    {
        avcodec_close(self->codec_context);
        free(self);
        return 4;
    }

    self->codec_context->channels = 2;
    self->codec_context->bit_rate = 12000 * 8;
    self->codec_context->sample_rate = 44100;
    //self->codec_context->sample_rate = 48000;
    self->codec_context->bits_per_raw_sample = 16;
    self->codec_context->block_align = 4;

    self->codec_context->profile = FF_PROFILE_AAC_LOW;

    //self->codec_context-> AV_SAMPLE_FMT_S16

    if (0)
    {
        unsigned short* ptr16;
        self->codec_context->extradata_size = 12 + 8;
        self->codec_context->extradata = calloc(1, self->codec_context->extradata_size);
	ptr16 = (unsigned short*)(self->codec_context->extradata);
	ptr16[0] = 0;
	ptr16[1] = 0xFE;
	ptr16[2] = 0;
    }

    error = AVCODEC_OPEN(self->codec_context, self->codec);
    if (error != 0)
    {
        avcodec_close(self->codec_context);
        free(self);
        return 5;
    }
    self->frame = AVCODEC_ALLOC_FRAME();
    *obj = self;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_delete(void* obj)
{
    struct avcodec_audio* self;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 0;
    }
    avcodec_close(self->codec_context);
    AVCODEC_FREE_FRAME(&(self->frame));
    free(self);
    return 0;
}

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
/* debian 6 */

/*****************************************************************************/
int
nrdp_avcodec_audio_decode(void* obj, void* cdata, int cdata_bytes,
                          int* cdata_bytes_processed, int* decoded)
{
    struct avcodec_audio* self;
    int len;
    int bytes_processed;
    unsigned char* src;
    int src_size;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    *cdata_bytes_processed = 0;
    *decoded = 0;
    bytes_processed = 0;
    src = cdata;
    src_size = cdata_bytes;
    while (src_size > 0)
    {
        self->dst_bytes = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        free(self->dst);
        self->dst = (short*) malloc(self->dst_bytes);
        len = avcodec_decode_audio2(self->codec_context,
                                    self->dst, &(self->dst_bytes),
                                    src, src_size);
        *decoded = self->dst_bytes > 0;
        if (len < 0)
        {
            return 1;
        }
        src_size -= len;
        src += len;
        bytes_processed += len;
        if (*decoded)   
        {
            *cdata_bytes_processed = bytes_processed;
            return 0;
        }
    }
    *cdata_bytes_processed = bytes_processed;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_get_frame_info(void* obj, int* channels, int* format,
                                  int* bytes)
{
    struct avcodec_audio* self;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    *channels = self->codec_context->channels;
    *format = 1; /* AV_SAMPLE_FMT_S16 */
    *bytes = self->dst_bytes;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_get_frame_data(void* obj, void* data, int data_bytes)
{
    struct avcodec_audio* self;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    memcpy(data, self->dst, data_bytes);
    return 0;
}

#else

/*****************************************************************************/
int
nrdp_avcodec_audio_decode(void* obj, void* cdata, int cdata_bytes,
                          int* cdata_bytes_processed, int* decoded)
{
    struct avcodec_audio* self;
    AVPacket pkt;
    int len;
    int bytes_processed;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    *cdata_bytes_processed = 0;
    *decoded = 0;
    bytes_processed = 0;
    av_init_packet(&pkt);
    pkt.data = cdata;
    pkt.size = cdata_bytes;
    while (pkt.size > 0)
    {
        len = avcodec_decode_audio4(self->codec_context,
                                    self->frame,
                                    decoded, &pkt);
        if (len < 0)
        {
            av_free_packet(&pkt);
            return 1;
        }
        pkt.size -= len;
        pkt.data += len;
        bytes_processed += len;
        if (*decoded)
        {
            *cdata_bytes_processed = bytes_processed;
            av_free_packet(&pkt);
            return 0;
        }
    }
    *cdata_bytes_processed = bytes_processed;
    av_free_packet(&pkt);
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_get_frame_info(void* obj, int* channels, int* format,
                                  int* bytes)
{
    struct avcodec_audio* self;
    int frame_size;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    if ((self->frame->format != AV_SAMPLE_FMT_S16) &&
        (self->frame->format != AV_SAMPLE_FMT_FLTP))
    {
        return 2;
    }
    frame_size = av_samples_get_buffer_size(NULL,
                                            self->codec_context->channels,
                                            self->frame->nb_samples,
                                            AV_SAMPLE_FMT_S16,
                                            1);
    *channels = self->codec_context->channels;
    *format = AV_SAMPLE_FMT_S16;
    *bytes = frame_size;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_audio_get_frame_data(void* obj, void* data, int data_bytes)
{
    struct avcodec_audio* self;

    self = (struct avcodec_audio*)obj;
    if (self == NULL)
    {
        return 1;
    }
    if (self->frame->format == AV_SAMPLE_FMT_S16)
    {
        memcpy(data, self->frame->data[0], data_bytes);
    }
    else if (self->frame->format == AV_SAMPLE_FMT_FLTP)
    {
        /* convert float to sint16 */
#if LIBAVCODEC_VERSION_MAJOR > 53
        float* src[8];
        short* dst;
        int index;

        src[0] = (float*)(self->frame->data[0]);
        src[1] = (float*)(self->frame->data[1]);
        src[2] = (float*)(self->frame->data[2]);
        src[3] = (float*)(self->frame->data[3]);
        src[4] = (float*)(self->frame->data[4]);
        src[5] = (float*)(self->frame->data[5]);
        dst = (short*)data;
        if (self->codec_context->channels == 6)
        {
            for (index = 0; index < self->frame->nb_samples; index++)
            {
                if (data_bytes < 12) /* 6 shorts */
                {
                    break;
                }
                dst[0] = src[0][index] * 32768;
                dst[1] = src[1][index] * 32768;
                dst[2] = src[2][index] * 32768;
                dst[3] = src[3][index] * 32768;
                dst[4] = src[4][index] * 32768;
                dst[5] = src[5][index] * 32768;
                dst += 6;
                data_bytes -= 12; /* 6 shorts */
            }
        }
        else if (self->codec_context->channels == 2)
        {
            for (index = 0; index < self->frame->nb_samples; index++)
            {
                if (data_bytes < 4) /* 2 shorts */
                {
                    break;
                }
                dst[0] = src[0][index] * 32768;
                dst[1] = src[1][index] * 32768;
                dst += 2;
                data_bytes -= 4; /* 2 shorts */
            }
        }
        else if (self->codec_context->channels == 1)
        {
            for (index = 0; index < self->frame->nb_samples; index++)
            {
                if (data_bytes < 2) /* 1 short */
                {
                    break;
                }
                dst[0] = src[0][index] * 32768;
                dst += 1;
                data_bytes -= 2; /* 1 short */
            }
        }
        else
        {
            return 1;
        }
#else
        /* self->frame->data only has 4 elements */
        return 1;
#endif
    }
    else
    {
        return 1;
    }
    return 0;
}

#endif

/*****************************************************************************/
int
nrdp_avcodec_video_create(void** obj, int codec_id)
{
    struct avcodec_video* self;
    int error;
    int avcodec_id;

    if (obj == NULL)
    {
        return 1;
    }
    avcodec_id = 0;
    switch (codec_id)
    {
        case VIDEO_CODEC_ID_MPEG2:
            avcodec_id = CODEC_ID_MPEG2VIDEO;
            break;
        default:
            return 6;
    }
    self = (struct avcodec_video*)malloc(sizeof(struct avcodec_video));
    if (self == NULL)
    {
        return 2;
    }
    memset(self, 0, sizeof(struct avcodec_video));
    self->codec_context = AVCODEC_ALLOC_CONTEXT;
    if (self->codec_context == NULL)
    {
        free(self);
        return 3;
    }
    self->codec = avcodec_find_decoder(avcodec_id);
    if (self->codec == NULL)
    {
        avcodec_close(self->codec_context);
        free(self);
        return 4;
    }
    error = AVCODEC_OPEN(self->codec_context, self->codec);
    if (error != 0)
    {
        avcodec_close(self->codec_context);
        free(self);
        return 5;
    }
    self->frame = AVCODEC_ALLOC_FRAME();
    *obj = self;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_video_delete(void* obj)
{
    struct avcodec_video* self;

    self = (struct avcodec_video*)obj;
    if (self == NULL)
    {
        return 0;
    }
    avcodec_close(self->codec_context);
    AVCODEC_FREE_FRAME(&(self->frame));
    free(self);
    return 0;
}

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
/* debian 6 */

/*****************************************************************************/
int
nrdp_avcodec_video_decode(void* obj, void* cdata, int cdata_bytes,
                          int* cdata_bytes_processed, int* decoded)
{
    struct avcodec_video* self;
    int len;
    int bytes_processed;
    unsigned char* src;
    int src_size;

    self = (struct avcodec_video*)obj;
    if (self == NULL)
    {
        return 1;
    }
    *cdata_bytes_processed = 0;
    *decoded = 0;
    bytes_processed = 0;
    src = cdata;
    src_size = cdata_bytes;
    while (src_size > 0)
    {
        len = avcodec_decode_video(self->codec_context,
                                   self->frame,
                                   decoded, src, src_size);
        if (len < 0)
        {
            return 1;
        }
        src_size -= len;
        src += len;
        bytes_processed += len;
        if (*decoded)
        {
            *cdata_bytes_processed = bytes_processed;
            return 0;
        }
    }
    *cdata_bytes_processed = bytes_processed;
    return 0;
}

#else

/*****************************************************************************/
int
nrdp_avcodec_video_decode(void* obj, void* cdata, int cdata_bytes,
                          int* cdata_bytes_processed, int* decoded)
{
    struct avcodec_video* self;
    AVPacket pkt;
    int len;
    int bytes_processed;

    self = (struct avcodec_video*)obj;
    if (self == NULL)
    {
        return 1;
    }
    *cdata_bytes_processed = 0;
    *decoded = 0;
    bytes_processed = 0;
    av_init_packet(&pkt);
    pkt.data = cdata;
    pkt.size = cdata_bytes;
    while (pkt.size > 0)
    {
        len = avcodec_decode_video2(self->codec_context,
                                    self->frame,
                                    decoded, &pkt);
        if (len < 0)
        {
            av_free_packet(&pkt);
            return 1;
        }
        pkt.size -= len;
        pkt.data += len;
        bytes_processed += len;
        if (*decoded)
        {
            *cdata_bytes_processed = bytes_processed;
            av_free_packet(&pkt);
            return 0;
        }
    }
    *cdata_bytes_processed = bytes_processed;
    av_free_packet(&pkt);
    return 0;
}

#endif

/*****************************************************************************/
int
nrdp_avcodec_video_get_frame_info(void* obj, int* width, int* height,
                                  int* format, int* bytes)
{
    struct avcodec_video* self;
    int frame_size;

    self = (struct avcodec_video*)obj;
    if (self == NULL)
    {
        return 1;
    }
    frame_size = avpicture_get_size(PIX_FMT_YUV420P,
                                    self->codec_context->width,
                                    self->codec_context->height);
    *width = self->codec_context->width;
    *height = self->codec_context->height;
    *format = PIX_FMT_YUV420P; /* 0 */
    *bytes = frame_size;
    return 0;
}

/*****************************************************************************/
int
nrdp_avcodec_video_get_frame_data(void* obj, void* data, int data_bytes)
{
    struct avcodec_video* self;
    AVFrame* frame;

    self = (struct avcodec_video*)obj;
    if (self == NULL)
    {
        return 1;
    }
    frame = AVCODEC_ALLOC_FRAME();
    avpicture_fill((AVPicture*)frame, data,
                   PIX_FMT_YUV420P,
                   self->codec_context->width,
                   self->codec_context->height);
    av_picture_copy((AVPicture*)frame,
                    (AVPicture*)(self->frame),
                    PIX_FMT_YUV420P,
                    self->codec_context->width,
                    self->codec_context->height);
    AVCODEC_FREE_FRAME(&frame);
    return 0;
}


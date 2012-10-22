
#include <libavcodec/avcodec.h>

#if LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR == 72 && LIBAVCODEC_VERSION_MICRO == 2
#define AVCODEC_J 1
#else
#define AVCODEC_LK 1
#endif

#ifdef AVCODEC_LK
#include <libavformat/avformat.h>
#endif

#include "xrdpvr_common.h"
#include "freerdp/types.h"
#include "freerdp/utils/svc_plugin.h"
#include "freerdp/utils/memory.h"

/* MajorType */
#define TSMF_MAJOR_TYPE_UNKNOWN             0
#define TSMF_MAJOR_TYPE_VIDEO               1
#define TSMF_MAJOR_TYPE_AUDIO               2

/* SubType */
#define TSMF_SUB_TYPE_UNKNOWN               0
#define TSMF_SUB_TYPE_WVC1                  1
#define TSMF_SUB_TYPE_WMA2                  2
#define TSMF_SUB_TYPE_WMA9                  3
#define TSMF_SUB_TYPE_MP3                   4
#define TSMF_SUB_TYPE_MP2A                  5
#define TSMF_SUB_TYPE_MP2V                  6
#define TSMF_SUB_TYPE_WMV3                  7
#define TSMF_SUB_TYPE_AAC                   8
#define TSMF_SUB_TYPE_H264                  9
#define TSMF_SUB_TYPE_AVC1                 10
#define TSMF_SUB_TYPE_AC3                  11
#define TSMF_SUB_TYPE_WMV2                 12
#define TSMF_SUB_TYPE_WMV1                 13
#define TSMF_SUB_TYPE_MP1V                 14
#define TSMF_SUB_TYPE_MP1A                 15
#define TSMF_SUB_TYPE_YUY2                 16
#define TSMF_SUB_TYPE_MP43                 17
#define TSMF_SUB_TYPE_MP4S                 18
#define TSMF_SUB_TYPE_MP42                 19

/* FormatType */
#define TSMF_FORMAT_TYPE_UNKNOWN            0
#define TSMF_FORMAT_TYPE_MFVIDEOFORMAT      1
#define TSMF_FORMAT_TYPE_WAVEFORMATEX       2
#define TSMF_FORMAT_TYPE_MPEG2VIDEOINFO     3
#define TSMF_FORMAT_TYPE_VIDEOINFO2         4
#define TSMF_FORMAT_TYPE_MPEG1VIDEOINFO     5

/* TS_MM_DATA_SAMPLE.SampleExtensions */
#define TSMM_SAMPLE_EXT_CLEANPOINT          0x00000001
#define TSMM_SAMPLE_EXT_DISCONTINUITY       0x00000002
#define TSMM_SAMPLE_EXT_INTERLACED          0x00000004
#define TSMM_SAMPLE_EXT_BOTTOMFIELDFIRST    0x00000008
#define TSMM_SAMPLE_EXT_REPEATFIELDFIRST    0x00000010
#define TSMM_SAMPLE_EXT_SINGLEFIELD         0x00000020
#define TSMM_SAMPLE_EXT_DERIVEDFROMTOPFIELD 0x00000040
#define TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS   0x00000080
#define TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS 0x00000100
#define TSMM_SAMPLE_EXT_ABSOLUTE_TIMESTAMPS 0x00000200

/* RDP_VIDEO_FRAME_EVENT.frame_pixfmt */
/* http://www.fourcc.org/yuv.php */
#define RDP_PIXFMT_I420     0x30323449
#define RDP_PIXFMT_YV12     0x32315659

typedef struct _XrdpMpegDecoder
{
	int             media_type;
	enum CodecID    codec_id;
	AVCodecContext* codec_context;
	AVCodec*        codec;
	AVFrame*        frame;
	int             prepared;

	uint8*          decoded_data;
	uint32          decoded_size;
	uint32          decoded_size_max;
} XrdpMpegDecoder;

/* @return opaque pointer on success, NULL on failure
 ******************************************************************************/
void *xrdpvr_init(void)
{
	XrdpMpegDecoder *decoder;

	printf("xrdpvr_init\n");
	decoder = (XrdpMpegDecoder *) calloc(1, sizeof(XrdpMpegDecoder));
	if (!decoder)
		return NULL;

#ifdef AVCODEC_LK
	av_register_all();
#else
	avcodec_init();
	avcodec_register_all();
#endif
	return decoder;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_deinit(void *decoder)
{
	if (decoder)
		free(decoder);

	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_init_context(void *dec)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

#ifdef AVCODEC_LK
	mdecoder->codec_context = avcodec_alloc_context3(NULL);
#else
	mdecoder->codec_context = avcodec_alloc_context();
#endif
	if (!mdecoder->codec_context)
	{
		DEBUG_WARN("avcodec_alloc_context failed.");
		return -1;
	}

	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_init_video_stream(void *dec, XrdpMediaInfo* vinfo)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

	printf("xrdpvr_init_video_stream\n");
	mdecoder->codec_context->width = vinfo->width;
	mdecoder->codec_context->height = vinfo->height;
	mdecoder->codec_context->bit_rate = vinfo->bit_rate;
	mdecoder->codec_context->time_base.den = vinfo->samples_per_sec_num;
	mdecoder->codec_context->time_base.num = vinfo->samples_per_sec_den;

	mdecoder->frame = avcodec_alloc_frame();

	return 0;
}

/*
 ******************************************************************************/
int xrdpvr_init_audio_stream(XrdpMpegDecoder* mdecoder, XrdpMediaInfo* ainfo)
{
	printf("### LK_TODO: not yet implemented\n");
	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_init_stream(void *dec, XrdpMediaInfo* media_info)
{
	uint32 		size;
	uint8* 		s;
	uint8* 		p;

	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

	mdecoder->codec = avcodec_find_decoder(mdecoder->codec_id);
	if (!mdecoder->codec)
	{
		DEBUG_WARN("avcodec_find_decoder() failed.");
		return -1;
	}

	mdecoder->codec_context->codec_id = mdecoder->codec_id;
	mdecoder->codec_context->codec_type = mdecoder->media_type;

	if (mdecoder->media_type == AVMEDIA_TYPE_VIDEO)
	{
		if (xrdpvr_init_video_stream((void *) mdecoder, media_info))
		{
			return -1;
		}
	}
	else if (mdecoder->media_type == AVMEDIA_TYPE_AUDIO)
	{
		if (!xrdpvr_init_audio_stream(mdecoder, media_info))
		{
			return -1;
		}
	}

	if (media_info->ExtraData)
	{
		if (media_info->SubType == TSMF_SUB_TYPE_AVC1 &&
			media_info->FormatType == TSMF_FORMAT_TYPE_MPEG2VIDEOINFO)
		{
			/* The extradata format that FFmpeg uses is following CodecPrivate in Matroska.
			   See http://haali.su/mkv/codecs.pdf */
			mdecoder->codec_context->extradata_size = media_info->ExtraDataSize + 8;
			mdecoder->codec_context->extradata = xzalloc(mdecoder->codec_context->extradata_size);
			p = mdecoder->codec_context->extradata;
			*p++ = 1; /* Reserved? */
			*p++ = media_info->ExtraData[8]; /* Profile */
			*p++ = 0; /* Profile */
			*p++ = media_info->ExtraData[12]; /* Level */
			*p++ = 0xff; /* Flag? */
			*p++ = 0xe0 | 0x01; /* Reserved | #sps */
			s = media_info->ExtraData + 20;
			size = ((uint32)(*s)) * 256 + ((uint32)(*(s + 1)));
			memcpy(p, s, size + 2);
			s += size + 2;
			p += size + 2;
			*p++ = 1; /* #pps */
			size = ((uint32)(*s)) * 256 + ((uint32)(*(s + 1)));
			memcpy(p, s, size + 2);
		}
		else
		{
			/* Add a padding to avoid invalid memory read in some codec */
			mdecoder->codec_context->extradata_size = media_info->ExtraDataSize + 8;
			mdecoder->codec_context->extradata = xzalloc(mdecoder->codec_context->extradata_size);
			memcpy(mdecoder->codec_context->extradata, media_info->ExtraData, media_info->ExtraDataSize);
			memset(mdecoder->codec_context->extradata + media_info->ExtraDataSize, 0, 8);
		}
	}

	if (mdecoder->codec->capabilities & CODEC_CAP_TRUNCATED)
		mdecoder->codec_context->flags |= CODEC_FLAG_TRUNCATED;

	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_prepare(void *dec)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

#ifdef AVCODEC_LK
	if (avcodec_open2(mdecoder->codec_context, mdecoder->codec, NULL) < 0)
#else
	if (avcodec_open(mdecoder->codec_context, mdecoder->codec) < 0)
#endif
	{
		DEBUG_WARN("avcodec_open2 failed.");
		return -1;
	}

	mdecoder->prepared = 1;

	return 0;
}

/* @return 0 on success, -1 on error
 ******************************************************************************/
int xrdpvr_set_format(void *dec, XrdpMediaInfo* media_info)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

	switch (media_info->MajorType)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			mdecoder->media_type = AVMEDIA_TYPE_VIDEO;
			break;

		case TSMF_MAJOR_TYPE_AUDIO:
			mdecoder->media_type = AVMEDIA_TYPE_AUDIO;
			break;

		default:
			printf("### xrdpvr_set_format() failed; wrong MajorType\n");
			return -1;
	}

	switch (media_info->SubType)
	{
		case TSMF_SUB_TYPE_WVC1:
			mdecoder->codec_id = CODEC_ID_VC1;
			break;

		case TSMF_SUB_TYPE_WMA2:
			mdecoder->codec_id = CODEC_ID_WMAV2;
			break;

		case TSMF_SUB_TYPE_WMA9:
			mdecoder->codec_id = CODEC_ID_WMAPRO;
			break;

		case TSMF_SUB_TYPE_MP3:
			mdecoder->codec_id = CODEC_ID_MP3;
			break;

		case TSMF_SUB_TYPE_MP2A:
			mdecoder->codec_id = CODEC_ID_MP2;
			break;

		case TSMF_SUB_TYPE_MP2V:
			mdecoder->codec_id = CODEC_ID_MPEG2VIDEO;
			break;

		case TSMF_SUB_TYPE_WMV3:
			mdecoder->codec_id = CODEC_ID_WMV3;
			break;

		case TSMF_SUB_TYPE_AAC:
			mdecoder->codec_id = CODEC_ID_AAC;
			/* For AAC the pFormat is a HEAACWAVEINFO struct, and the codec data
			   is at the end of it. See
			   http://msdn.microsoft.com/en-us/library/dd757806.aspx */
			if (media_info->ExtraData)
			{
				media_info->ExtraData += 12;
				media_info->ExtraDataSize -= 12;
			}
			break;

		case TSMF_SUB_TYPE_H264:
		case TSMF_SUB_TYPE_AVC1:
			mdecoder->codec_id = CODEC_ID_H264;
			break;

		case TSMF_SUB_TYPE_AC3:
			mdecoder->codec_id = CODEC_ID_AC3;
			break;

		default:
			printf("### xrdpvr_set_format() failed; wrong SubType\n");
			return -1;
	}

	if (xrdpvr_init_context((void *) mdecoder))
	{
		return -1;
	}

	if (xrdpvr_init_stream((void *) mdecoder, media_info))
	{
		return -1;
	}

	if (xrdpvr_prepare((void *) mdecoder))
	{
		return -1;
	}

	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_decode_video(void *dec, uint32 data_size, const uint8* data,
						uint32 extensions)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

	int 		decoded;
	int 		len;
	AVFrame* 	frame;
	int 		ret = 0;

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR <= 20)
	len = avcodec_decode_video(mdecoder->codec_context, mdecoder->frame, &decoded, data, data_size);
#else
	{
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = (uint8*) data;
		pkt.size = data_size;

#if 0
		if (extensions & TSMM_SAMPLE_EXT_CLEANPOINT)
			pkt.flags |= AV_PKT_FLAG_KEY;
#endif

		len = avcodec_decode_video2(mdecoder->codec_context, mdecoder->frame, &decoded, &pkt);
	}
#endif

	printf("xrdpvr_decode_video: avcodec_decode_video2 returned %d\n", len);
	if (len < 0)
	{
		DEBUG_WARN("data_size %d, avcodec_decode_video failed (%d)", data_size, len);
		ret = -1;
	}
	else if (!decoded)
	{
		DEBUG_WARN("data_size %d, no frame is decoded.", data_size);
		ret = -1;
	}
	else
	{
		DEBUG_SVC("linesize[0] %d linesize[1] %d linesize[2] %d linesize[3] %d "
			"pix_fmt %d width %d height %d",
			mdecoder->frame->linesize[0], mdecoder->frame->linesize[1],
			mdecoder->frame->linesize[2], mdecoder->frame->linesize[3],
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		mdecoder->decoded_size = avpicture_get_size(mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);
		mdecoder->decoded_data = xzalloc(mdecoder->decoded_size);
		frame = avcodec_alloc_frame();
		avpicture_fill((AVPicture *) frame, mdecoder->decoded_data,
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		av_picture_copy((AVPicture *) frame, (AVPicture *) mdecoder->frame,
			mdecoder->codec_context->pix_fmt,
			mdecoder->codec_context->width, mdecoder->codec_context->height);

		av_free(frame);
	}

	return ret;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_decode_audio(void *dec, uint32 data_size, const uint8* data,
						uint32 extensions)
{
	/* XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec; */

	printf("### not yet implemented\n");
	return 0;
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_decode(void* dec, const uint32 data_size, const uint8* data,
				  uint32 extensions)
{
	XrdpMpegDecoder* mdecoder = (XrdpMpegDecoder *) dec;

	if (mdecoder->decoded_data)
	{
		free(mdecoder->decoded_data);
		mdecoder->decoded_data = NULL;
	}
	mdecoder->decoded_size = 0;

	switch (mdecoder->media_type)
	{
		case AVMEDIA_TYPE_VIDEO:
			return xrdpvr_decode_video(mdecoder, data_size, data, extensions);

		case AVMEDIA_TYPE_AUDIO:
			return xrdpvr_decode_audio(mdecoder, data_size, data, extensions);

		default:
			DEBUG_WARN("unknown media type.");
			return -1;
	}
}

/*
 ******************************************************************************/
uint8* xrdpv_get_decoded_data(void* dec, uint32* size)
{
	XrdpMpegDecoder* mdecoder = dec;
	uint8* buf;

	*size = mdecoder->decoded_size;
	buf = mdecoder->decoded_data;
	mdecoder->decoded_data = NULL;
	mdecoder->decoded_size = 0;
	return buf;
}

/*
 ******************************************************************************/
uint32 xrdpvr_get_decoded_format(void* dec)
{
	XrdpMpegDecoder* mdecoder = dec;

	switch (mdecoder->codec_context->pix_fmt)
	{
		case PIX_FMT_YUV420P:
			return RDP_PIXFMT_I420;

		default:
			DEBUG_WARN("unsupported pixel format %u",
					   mdecoder->codec_context->pix_fmt);
			return (uint32) -1;
	}
}

/* @return 0 on success, -1 on failure
 ******************************************************************************/
int xrdpvr_get_decoded_dimension(void* dec, uint32* width,
								 uint32* height)
{
	XrdpMpegDecoder* mdecoder = dec;

	if (mdecoder->codec_context->width > 0 && mdecoder->codec_context->height > 0)
	{
		*width = mdecoder->codec_context->width;
		*height = mdecoder->codec_context->height;
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
 ******************************************************************************/
void xrdpvr_free(void* dec)
{
	XrdpMpegDecoder* mdecoder = dec;

	if (mdecoder->frame)
		av_free(mdecoder->frame);

	if (mdecoder->decoded_data)
		free(mdecoder->decoded_data);

	if (mdecoder->codec_context)
	{
		if (mdecoder->prepared)
			avcodec_close(mdecoder->codec_context);

		if (mdecoder->codec_context->extradata)
			free(mdecoder->codec_context->extradata);

		av_free(mdecoder->codec_context);
	}
	free(mdecoder);
}

#if 0
ITSMFDecoder*
TSMFDecoderEntry(void)
{
	TSMFFFmpegDecoder * decoder;

	if (!initialized)
	{
		avcodec_register_all();
		initialized = TRUE;
	}

	decoder = xnew(TSMFFFmpegDecoder);

	decoder->iface.SetFormat = tsmf_ffmpeg_set_format;
	decoder->iface.Decode = tsmf_ffmpeg_decode;
	decoder->iface.GetDecodedData = tsmf_ffmpeg_get_decoded_data;
	decoder->iface.GetDecodedFormat = tsmf_ffmpeg_get_decoded_format;
	decoder->iface.GetDecodedDimension = tsmf_ffmpeg_get_decoded_dimension;
	decoder->iface.Free = tsmf_ffmpeg_free;

	return (ITSMFDecoder*) decoder;
}
#endif

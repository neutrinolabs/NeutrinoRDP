#ifndef __XRDPVR_COMMON_H__
#define __XRDPVR_COMMON_H__

#include "freerdp/types.h"

typedef struct _XrdpMediaInfo
{
	int 			MajorType;
	int 			SubType;
	int 			FormatType;
	int				width;
	int				height;
	int				bit_rate;
	int				samples_per_sec_num;
	int				samples_per_sec_den;
    uint32			ExtraDataSize;
	uint8* 			ExtraData;
	uint32			stream_id;
} XrdpMediaInfo;

void *xrdpvr_init(void);
int xrdpvr_deinit(void *decoder);
int xrdpvr_init_context(void *dec);
int xrdpvr_init_video_stream(void *dec, XrdpMediaInfo* vinfo);
//int xrdpvr_init_audio_stream(XrdpMpegDecoder* mdecoder, XrdpMediaInfo* ainfo);
int xrdpvr_init_stream(void *dec, XrdpMediaInfo* media_info);
int xrdpvr_prepare(void *dec);
int xrdpvr_set_format(void *dec, XrdpMediaInfo* media_info);

int xrdpvr_decode_video(void* dec, uint32 data_size, const uint8* data, uint32 extensions);
int xrdpvr_decode_audio(void* dec, uint32 data_size, const uint8* data, uint32 extensions);
int xrdpvr_decode(void* dec, uint32 data_size, const uint8* data, uint32 extensions);
uint8* xrdpv_get_decoded_data(void* dec, uint32* size);
uint32 xrdpvr_get_decoded_format(void* dec);
int xrdpvr_get_decoded_dimension(void* dec, uint32* width, uint32* height);
void xrdpvr_free(void* dec);

#endif

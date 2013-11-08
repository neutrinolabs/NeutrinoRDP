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

#ifndef __XRDPVR_PLAYER_H
#define __XRDPVR_PLAYER_H

#include <freerdp/types.h>

extern int g_meta_data_fd;

void  *init_player(void *plugin, char *filename);
void   deinit_player(void *psi);
int    process_video(void *vp, uint8 *data, int data_len);
int    process_audio(void *vp, uint8 *data, int data_len);
uint8 *get_decoded_audio_data(void *vp, uint32 *size);
void   get_audio_config(void *vp, int *samp_per_sec, int *num_channels, int *bits_per_samp);
void   set_audio_config(void* vp, char* extradata, int extradata_size, int sample_rate, int bit_rate, int channels, int block_align);
void   set_video_config(void *vp);
void   set_geometry(void *g_psi, int xpos, int ypos, int width, int height);
void   set_volume(void *g_psi, int volume);

#endif

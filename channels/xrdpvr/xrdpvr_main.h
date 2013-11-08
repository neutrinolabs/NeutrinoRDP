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

#ifndef __XRDPVR_MAIN_H
#define __XRDPVR_MAIN_H

typedef struct xrdpvr_plugin xrdpvrPlugin;

#define CMD_SET_VIDEO_FORMAT        1
#define CMD_SET_AUDIO_FORMAT        2
#define CMD_SEND_VIDEO_DATA         3
#define CMD_SEND_AUDIO_DATA         4
#define CMD_CREATE_META_DATA_FILE   5
#define CMD_CLOSE_META_DATA_FILE    6
#define CMD_WRITE_META_DATA         7
#define CMD_DEINIT_XRDPVR           8
#define CMD_SET_GEOMETRY            9
#define CMD_SET_VOLUME              10
#define CMD_INIT_XRDPVR             11

/* TODO need to support Windows paths */
#define META_DATA_FILEAME           "/tmp/xrdpvr_metadata.dat"

#endif /* __XRDPVR_MAIN_H */

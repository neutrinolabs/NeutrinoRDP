/**
 * FreeRDP: A Remote Desktop Protocol client.
 * ThinClient Utilities channel
 *
 * Copyright 2013 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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

#ifndef _TCUTILS_MAIN_H_
#define _TCUTILS_MAIN_H_

#include <sys/mount.h>
#include <errno.h>
#include <stdlib.h>

/* module based logging */
#define LOG_ERROR   0
#define LOG_INFO    1
#define LOG_DEBUG   2
#define LOG_LEVEL   LOG_ERROR

#define log_error(_fmt, ...)  printf("TCUTILS    %s: %d: ERROR: " _fmt, __func__, __LINE__, ## __VA_ARGS__)
#define log_always(_fmt, ...) printf("TCUTILS    %s: %d: " _fmt, __func__, __LINE__, ## __VA_ARGS__)

#define log_info(_fmt, ...)                                                     	\
{                                                                               	\
	if (LOG_INFO <= LOG_LEVEL)                                                  	\
	{                                                                           	\
		printf("TCUTILS    %s: %d: " _fmt, __func__, __LINE__, ## __VA_ARGS__); \
	}                                                                           	\
}

#define log_debug(_fmt, ...)                                                    	\
{                                                                               	\
	if (LOG_DEBUG <= LOG_LEVEL)                                                 	\
	{                                                                           	\
		printf("TCUTILS    %s: %d: " _fmt, __func__, __LINE__, ## __VA_ARGS__); \
	}                                                                           	\
}


typedef struct tcutils_plugin tcutilsPlugin;

/* list of commands we support */
enum TCU_COMMANDS
{
	TCU_CMD_GET_MOUNT_LIST = 1,
	TCU_CMD_UNMOUNT_DEVICE,
	TCU_CMD_LAST,
};

/* umount error codes */
enum TCU_UMOUNT_ERROR
{
	UMOUNT_SUCCESS = 0,
	UMOUNT_BUSY,
	UMOUNT_NOT_MOUNTED,
	UMOUNT_OP_NOT_PERMITTED,
	UMOUNT_PERMISSION_DENIED,
	UMOUNT_UNKNOWN
};

/* forward declarations */
void tcutils_process_receive(rdpSvcPlugin *plugin_in, STREAM *data_in);

/******************************************************************************
**                                                                           **
**                       functions local to this file                        **
**                                                                           **
******************************************************************************/

static int  tcutils_init();
static int  tcutils_deinit();
static void tcutils_process_connect(rdpSvcPlugin* plugin_p);
static void tcutils_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event);
static void tcutils_process_terminate(rdpSvcPlugin* plugin_p);

static int tcutils_get_mount_list(tcutilsPlugin* plugin, STREAM* data_in,
				  int cmd_len, int pkt_len);

static int tcutils_unmount_device(tcutilsPlugin* plugin, STREAM* data_in,
				  int cmd_len, int pkt_len);

/******************************************************************************
**                                                                           **
**                  misc helper functions local to this file                 **
**                                                                           **
******************************************************************************/

static int tcutils_insert_mount_points(char* data_buf, int* num_entries,
				       int* bytes_inserted);

#endif /* _TCUTILS_MAIN_H_ */

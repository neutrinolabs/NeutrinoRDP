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

#include <freerdp/constants.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include "tcutils_main.h"

typedef struct tcutils_plugin tcutilsPlugin;

struct tcutils_plugin
{
	rdpSvcPlugin	plugin;
};

/* globals */
int  (*g_tcu_commands[TCU_CMD_LAST])();
int  g_inited = 0;

/**
 * Process incoming data
 *
 * @param  plugin_in  a pointer to our plugin
 * @param  data_in    data received from server
 *
 * Data format:
 * 	4 bytes    num bytes required for this command
 * 	n bytes    bytes for above command
 *****************************************************************************/

void
tcutils_process_receive(rdpSvcPlugin* plugin_in, STREAM* data_in)
{
	tcutilsPlugin* plugin = (tcutilsPlugin *) plugin_in;

	uint32 cmd;
	int    rv;
	uint32 cmd_len; /* num bytes required by current cmd */
	int    pkt_len; /* num of bytes in this pkt          */

	log_debug("entered\n");

	if (plugin == NULL)
	{
		log_error("plugin is NULL\n");
		goto done;
	}

	if (!g_inited)
	{
		log_debug("tcutils NOT inited!\n");
		goto done;
	}

	pkt_len = stream_get_size(data_in);

	/* get num bytes required by current cmd */
	stream_read_uint32(data_in, cmd_len);
	if(cmd_len + 4 != pkt_len)
	{
		log_debug("got only %d/%d bytes; not processing this pkt\n",
                          pkt_len - 4, cmd_len);
		goto done;
	}

	/* get command */
	stream_read_uint32(data_in, cmd);

	if ((cmd < 0) || (cmd >= TCU_CMD_LAST))
	{
		log_error("got unknown command: 0x%x %d(.)\n", cmd, cmd);
		goto done;
	}

	rv = (*g_tcu_commands[cmd])(plugin, data_in, cmd_len, pkt_len);

done:

	stream_free(data_in);
}

/******************************************************************************
**                                                                           **
**                       functions local to this file                        **
**                                                                           **
******************************************************************************/

/**
 *****************************************************************************/

static int
tcutils_init()
{
	log_debug("entered\n");

	g_tcu_commands[TCU_CMD_GET_MOUNT_LIST] = tcutils_get_mount_list;
	g_inited = 1;

	return 0;
}

/**
 *****************************************************************************/

static int
tcutils_deinit()
{
	log_debug("entered\n");

	return 0;
}

/**
 *****************************************************************************/

static void
tcutils_process_connect(rdpSvcPlugin* plugin_p)
{
	tcutilsPlugin* plugin = (tcutilsPlugin *) plugin_p;

	log_debug("entered\n");

	if (plugin == NULL)
	{
		log_error("plugin is NULL!\n");
		return;
	}

#if 0
	/* if you want a call from channel thread once a second, do this */
	plugin_p->interval_ms = 1000000;
	plugin_p->interval_callback = tcutils_process_interval;
#endif

	tcutils_init();
}

/**
 *****************************************************************************/

static void
tcutils_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	log_debug("entered\n");

	/* events comming from main freerdp window to plugin */
	/* send them back with svc_plugin_send_event */

	freerdp_event_free(event);
}

/**
 *
 *****************************************************************************/

static void
tcutils_process_terminate(rdpSvcPlugin* plugin_p)
{
	tcutilsPlugin* plugin = (tcutilsPlugin *) plugin_p;

	log_debug("entered\n");

	if (plugin == NULL)
	{
		log_error("plugin is NULL!\n");
		return;
	}

	tcutils_deinit();
	xfree(plugin);
}

/**
 * Return list of mounted file systems
 *****************************************************************************/

static int
tcutils_get_mount_list(tcutilsPlugin* plugin, STREAM* data_in,
		       int cmd_len, int pkt_len)
{
	STREAM* data_out;
	int     num_entries;
	int     bytes_inserted;
	int     rv;

	log_debug("entered\n");

	data_out = stream_new(1024 * 8);

	/*
	 * command format
	 * 	4 bytes  pkt_len     number of bytes in this pkt
	 * 	4 bytes  cmd         TCU_CMD_GET_MOUNT_LIST
	 * 	1 byte   nentries    number of entries in this pkt
	 * 	n bytes  entry_list  nentries null terminated strings
	 */

	rv = tcutils_insert_mount_points((char *) &data_out->data[9],
					 &num_entries, &bytes_inserted);

	log_debug("tcutils_insert_mount_points() reted %d\n", rv);

	if (rv)
	{
		/* command failed, zero entries inserted */
		log_debug("zero entries inserted in buf\n");
		stream_write_uint32(data_out, 9);
		stream_write_uint32(data_out, TCU_CMD_GET_MOUNT_LIST);
		stream_write_uint8(data_out, 0);
		goto done;
	}

	log_debug("entries_inserted=%d bytes_inserted=%d\n", num_entries, bytes_inserted);

	stream_write_uint32(data_out, 9 + bytes_inserted);
	stream_write_uint32(data_out, TCU_CMD_GET_MOUNT_LIST);
	stream_write_uint8(data_out, num_entries);
	data_out->p += bytes_inserted;
done:
	svc_plugin_send((rdpSvcPlugin*) plugin, data_out);
	return 0;
}

/******************************************************************************
**                                                                           **
**                  misc helper functions local to this file                 **
**                                                                           **
******************************************************************************/

/**
 * Insert list of mounted file systems and their mount points into buffer
 *
 * @param  data_buf        insert data here
 * @param  num_entries     return number of entries inserted into data_buf
 * @param  bytes_inserted  return number of bytes inerted into data_buf
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

static int
tcutils_insert_mount_points(char* data_buf, int* num_entries, int* bytes_inserted)
{
    FILE* fp;
    char* cptr;
    char  buf[2048];
    int   len;
    int   total_len = 0;
    int   entries = 0;

    *bytes_inserted = 0;
    *num_entries = 0;
    if ((fp = popen("grep '^/dev/[a-zA-Z]*[0-9] /' /proc/mounts | awk '{print $1 \" \" $2}'", "r")) == NULL)
        return -1;

    while (fgets(buf, 2048, fp) != NULL)
    {
        /* remove terminating newline */
        if ((cptr = strstr(buf, "\n")) != NULL)
            *cptr = 0;

        len = strlen(buf) + 1;
        strcat(data_buf, buf);
        data_buf += len;
        total_len += len;
        entries++;

        log_debyg("%s\n", buf);
    }

    *num_entries = entries;
    *bytes_inserted = total_len;

    pclose(fp);
    return 0;
}

DEFINE_SVC_PLUGIN(tcutils, "tcutils",
                  CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)

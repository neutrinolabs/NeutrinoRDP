/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Network Transport Layer
 *
 * Copyright 2011 Vic Lee
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>

#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
#include "credssp.h"
#include "transport.h"

#define BUFFER_SIZE (16384 * 2)

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)
#define LHEXDUMP(_level, _args) \
  do { if (_level < LLOG_LEVEL) { freerdp_hexdump _args ; } } while (0)

STREAM* transport_recv_stream_init(rdpTransport* transport, int size)
{
	STREAM* s = transport->recv_stream;
	stream_check_size(s, size);
	stream_set_pos(s, 0);
	return s;
}

STREAM* transport_send_stream_init(rdpTransport* transport, int size)
{
	STREAM* s = transport->send_stream;
	stream_check_size(s, size);
	stream_set_pos(s, 0);
	return s;
}

boolean transport_tsg_connect(rdpTransport* transport, const char* hostname, uint16 port)
{
	rdpTsg* tsg = tsg_new(transport->settings);
	tsg->transport = transport;
	transport->tsg = tsg;

	LLOGLN(10, ("transport_tsg_connect:"));
	if (transport->tls_in == NULL)
	{
		LLOGLN(10, ("transport_tsg_connect: tls_in calling tls_new"));
		transport->tls_in = tls_new(transport->settings);
	}
	transport->tls_in->sockfd = transport->tcp_in->sockfd;
	if (transport->tls_out == NULL)
	{
		LLOGLN(10, ("transport_tsg_connect: tls_out calling tls_new"));
		transport->tls_out = tls_new(transport->settings);
	}
	transport->tls_out->sockfd = transport->tcp_out->sockfd;
	if (tls_connect(transport->tls_in) == false)
	{
		LLOGLN(0, ("transport_tsg_connect: tls_in tls_connect failed"));
		return false;
	}
	LLOGLN(10, ("transport_tsg_connect: tls_in tls_connect ok"));
	if (tls_connect(transport->tls_out) == false)
	{
		LLOGLN(0, ("transport_tsg_connect: tls_out tls_connect failed"));
		return false;
	}
	LLOGLN(10, ("transport_tsg_connect: tls_out tls_connect ok"));
	if (!tsg_connect(tsg, hostname, port))
	{
		LLOGLN(0, ("transport_tsg_connect: tsg_connect failed"));
		return false;
	}
	LLOGLN(10, ("transport_tsg_connect: tsg_connect ok"));
	LLOGLN(10, ("transport_tsg_connect: ok"));
	return true;
}

tbool transport_connect(rdpTransport* transport, const char* hostname, uint16 port)
{
	tbool ok;

	LLOGLN(10, ("transport_connect:"));
	if (transport->settings->tsg)
	{
		LLOGLN(10, ("transport_connect: settings->tsg set"));
		transport->layer = TRANSPORT_LAYER_TSG;
		transport->tcp_out = tcp_new(transport->settings);
		ok = tcp_connect(transport->tcp_in, transport->settings->tsg_server, 443);
		if (ok)
		{
			ok = tcp_connect(transport->tcp_out, transport->settings->tsg_server, 443);
			if (ok)
			{
				ok = transport_tsg_connect(transport, hostname, port);
				if (ok)
				{
					LLOGLN(0, ("transport_connect: gw connect ok"));
				}
				else
				{
					LLOGLN(0, ("transport_connect: gw transport_tsg_connect failed"));
				}
			}
			else
			{
				LLOGLN(0, ("transport_connect: gw tcp_connect tcp_out failed"));
			}
		}
		else
		{
			LLOGLN(0, ("transport_connect: gw tcp_connect tcp_in failed"));
		}
		return ok;
	}
	else
	{
		LLOGLN(10, ("transport_connect: settings->tsg not set"));
		transport->tcp_out = transport->tcp_in;
		return tcp_connect(transport->tcp_in, hostname, port);
	}
}

void transport_attach(rdpTransport* transport, int sockfd)
{
	transport->tcp_in->sockfd = sockfd;
}

tbool transport_disconnect(rdpTransport* transport)
{
	if (transport->layer == TRANSPORT_LAYER_TLS)
		tls_disconnect(transport->tls_in);
	return tcp_disconnect(transport->tcp_in);
}

tbool transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */

	return true;
}

tbool transport_connect_tls(rdpTransport* transport)
{
	if (transport->tls_in == NULL)
		transport->tls_in = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->tls_in->sockfd = transport->tcp_in->sockfd;

	if (tls_connect(transport->tls_in) == false)
		return false;

	return true;
}

tbool transport_connect_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;

	if (transport->tls_in == NULL)
		transport->tls_in = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->tls_in->sockfd = transport->tcp_in->sockfd;

	if (tls_connect(transport->tls_in) == false)
		return false;

	/* Network Level Authentication */

	if (transport->settings->authentication == false)
		return true;

	settings = transport->settings;
	instance = (freerdp*) settings->instance;

	if (transport->credssp == NULL)
		transport->credssp = credssp_new(instance, transport->tls_in, settings);

	if (credssp_authenticate(transport->credssp) < 0)
	{
		printf("Authentication failure, check credentials.\n"
			"If credentials are valid, the NTLMSSP implementation may be to blame.\n");

		credssp_free(transport->credssp);
		return false;
	}

	credssp_free(transport->credssp);

	return true;
}

tbool transport_accept_rdp(rdpTransport* transport)
{
	/* RDP encryption */

	return true;
}

tbool transport_accept_tls(rdpTransport* transport)
{
	if (transport->tls_in == NULL)
		transport->tls_in = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->tls_in->sockfd = transport->tcp_in->sockfd;

	if (tls_accept(transport->tls_in, transport->settings->cert_file, transport->settings->privatekey_file) == false)
		return false;

	return true;
}

tbool transport_accept_nla(rdpTransport* transport)
{
	if (transport->tls_in == NULL)
		transport->tls_in = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->tls_in->sockfd = transport->tcp_in->sockfd;

	if (tls_accept(transport->tls_in, transport->settings->cert_file, transport->settings->privatekey_file) == false)
		return false;

	/* Network Level Authentication */

	if (transport->settings->authentication == false)
		return true;

	/* Blocking here until NLA is complete */

	return true;
}

/* will not return until all data is read if transport->blocking is set */
/* else returns 0 if call would block */
int transport_read_layer(rdpTransport* transport, uint8* data, int bytes)
{
	int read = 0;
	int status = -1;

	LLOGLN(10, ("transport_read_layer:"));
	while (read < bytes)
	{
		switch (transport->layer)
		{
			case TRANSPORT_LAYER_TSG:
			case TRANSPORT_LAYER_TLS:
				status = tls_read(transport->tls_in, data + read, bytes - read);
				break;
			case TRANSPORT_LAYER_TCP:
				status = tcp_read(transport->tcp_in, data + read, bytes - read);
				break;
			//case TRANSPORT_LAYER_TSG:
				//status = tsg_read(transport->tsg, data + read, bytes - read);
				//break;
			default:
				LLOGLN(0, ("transport_read_layer: unknown layer %d", transport->layer));
				break;
		}

		/* blocking means that we can't continue until this is read
		   it's not tcp blocking */

		if (transport->blocking == false)
			return status;

		if (status < 0)
			return status;

		read += status;

		if (status == 0)
		{
			switch (transport->layer)
			{
				case TRANSPORT_LAYER_TSG:
				case TRANSPORT_LAYER_TLS:
					tcp_can_recv(transport->tls_out->sockfd, 100);
					break;
				case TRANSPORT_LAYER_TCP:
					tcp_can_recv(transport->tcp_out->sockfd, 100);
					break;
				default:
					freerdp_usleep(transport->usleep_interval);
					break;
			}
		}
	}

	return read;
}

int transport_read(rdpTransport* transport, STREAM* s)
{
	int status;
	int pdu_bytes;
	int stream_bytes;
	int transport_status;
	int header_bytes;
	tbool got_whole_pdu;

	LLOGLN(10, ("transport_read: blocking %d", transport->blocking));
	transport_status = 0;

	/* first check if we have header */
	stream_bytes = stream_get_length(s);

	header_bytes = transport->layer == TRANSPORT_LAYER_TSG ? 10 : 4;

	if (stream_bytes < header_bytes)
	{
		LLOGLN(10, ("transport_read: transport_read_layer 1st call"));
		status = transport_read_layer(transport, s->data + stream_bytes,
				header_bytes - stream_bytes);

		if (status < 0)
		{
			LLOGLN(0, ("transport_read: transport_read_layer failed"));
			return status;
		}

		transport_status += status;

		if ((status + stream_bytes) < header_bytes)
		{
			LLOGLN(10, ("transport_read: not enough for header"));
			return transport_status;
		}

		stream_bytes += status;
	}

	pdu_bytes = 0;

	if (transport->layer == TRANSPORT_LAYER_TSG)
	{
		pdu_bytes = s->data[8];
		pdu_bytes |= s->data[9] << 8;
	}
	else
	{
		/* if header is present, read in exactly one PDU */
		if (s->data[0] == 0x03)
		{
			/* TPKT header */
			pdu_bytes = (s->data[2] << 8) | s->data[3];
		}
		else if (s->data[0] == 0x30)
		{
			/* TSRequest (NLA) */
			if (s->data[1] & 0x80)
			{
				if ((s->data[1] & ~(0x80)) == 1)
				{
					pdu_bytes = s->data[2];
					pdu_bytes += 3;
				}
				else if ((s->data[1] & ~(0x80)) == 2)
				{
					pdu_bytes = (s->data[2] << 8) | s->data[3];
					pdu_bytes += 4;
				}
				else
				{
					printf("Error reading TSRequest!\n");
				}
			}
			else
			{
				pdu_bytes = s->data[1];
				pdu_bytes += 2;
			}
		}
		else
		{
			/* Fast-Path Header */
			if (s->data[1] & 0x80)
				pdu_bytes = ((s->data[1] & 0x7f) << 8) | s->data[2];
			else
				pdu_bytes = s->data[1];
		}
	}

	LLOGLN(10, ("transport_read: transport_read_layer 2nd call"));
	status = transport_read_layer(transport, s->data + stream_bytes,
			pdu_bytes - stream_bytes);

	if (status < 0)
		return status;

	transport_status += status;

	got_whole_pdu = stream_bytes + status >= pdu_bytes;

#ifdef WITH_DEBUG_TRANSPORT
	/* dump when whole PDU is read */
	if (got_whole_pdu)
	{
		printf("Local < Remote\n");
		freerdp_hexdump(s->data, pdu_bytes);
	}
#endif

#if 1
	if (transport->blocking)
	{
		if ((transport->layer == TRANSPORT_LAYER_TSG) && got_whole_pdu)
		{
			//uint8* jj = s->p; // why don't work
			LLOGLN(10, ("transport_read: calling tsg_skip_pdu"));
			s->p = s->data;
			if (tsg_skip_pdu(transport->tsg, s))
			{
				LLOGLN(10, ("transport_read: skipping"));
				s->p = s->data;
				return transport_read(transport, s);
			}
			//s->p = jj;
		}
	}
#endif

	LLOGLN(10, ("transport_read: returning %d", transport_status));
	return transport_status;
}

static int transport_read_nonblocking(rdpTransport* transport)
{
	int status;

	stream_check_size(transport->recv_buffer, 32 * 1024);
	status = transport_read(transport, transport->recv_buffer);
	if (status <= 0)
	{
		/* error or blocking */
		return status;
	}
	stream_seek(transport->recv_buffer, status);
	return status;
}

int transport_write(rdpTransport* transport, STREAM* s)
{
	int status = -1;
	int length;

	LLOGLN(10, ("transport_write:"));

	length = stream_get_length(s);
	stream_set_pos(s, 0);

#ifdef WITH_DEBUG_TRANSPORT
	if (length > 0)
	{
		printf("Local > Remote\n");
		freerdp_hexdump(s->data, length);
	}
#endif

	while (length > 0)
	{
		switch (transport->layer)
		{
			case TRANSPORT_LAYER_TLS:
				status = tls_write(transport->tls_in, stream_get_tail(s), length);
				break;
			case TRANSPORT_LAYER_TCP:
				status = tcp_write(transport->tcp_in, stream_get_tail(s), length);
				break;
			case TRANSPORT_LAYER_TSG:
				status = tsg_write(transport->tsg, stream_get_tail(s), length);
				break;
			default:
				LLOGLN(0, ("transport_write: unknown transport->layer %d", transport->layer));
				break;
		}

		if (status < 0)
			break; /* error occurred */

		if (status == 0)
		{
			/* blocking while sending */
			freerdp_usleep(transport->usleep_interval);
		}

		length -= status;
		stream_seek(s, status);
	}

	if (status < 0)
	{
		/* A write error indicates that the peer has dropped the connection */
		transport->layer = TRANSPORT_LAYER_CLOSED;
	}

	return status;
}

void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount)
{
	LLOGLN(10, ("transport_get_fds:"));
	rfds[*rcount] = (void*)(long)(transport->tcp_out->sockfd);
	(*rcount)++;
	LLOGLN(10, ("  fd1 %d", transport->tcp_out->sockfd));
	if (transport->tcp_in == transport->tcp_out)
	{
	}
	else
	{
		rfds[*rcount] = (void*)(long)(transport->tcp_in->sockfd);
		(*rcount)++;
		LLOGLN(10, ("  fd1 %d", transport->tcp_in->sockfd));
	}
}

int get_rdp_pdu_length(uint8* data)
{
	int pdu_bytes;

	if (data[0] == 0x03)
	{
		/* TPKT header */
		pdu_bytes = (data[2] << 8) | data[3];
	}
	else
	{
		/* Fast-Path Header */
		if (data[1] & 0x80)
		{
			pdu_bytes = ((data[1] & 0x7f) << 8) | data[2];
		}
		else
		{
			pdu_bytes = data[1];
		}
	}
	return pdu_bytes;
}

static int do_callback(rdpTransport* transport, STREAM* s)
{
	int rv;

	rv = 0;
	transport->level++;
	if (transport->recv_callback(transport, s, transport->recv_extra) == false)
	{
		LLOGLN(0, ("transport_check_fds: transport->recv_callback failed"));
		rv = -1;
	}
	transport->level--;
	return rv;
}

int transport_check_fds(rdpTransport* transport)
{
	int pos;
	int status;
	int rdp_pdu_length;
	int extra_bytes;
	uint16 length;
	STREAM* proc_s;
	STREAM* s;

	int ptype;
	int pfc_flags;
	int frag_length;
	int auth_length;
	int call_id;
	int alloc_hint;
	int auth_pad_length;

	LLOGLN(10, ("transport_check_fds:"));

	/* test for nested calls */
	if (transport->level != 0)
	{
		LLOGLN(0, ("transport_check_fds: error, nested calls"));
		return -1;
	}

	status = transport_read_nonblocking(transport);

	if (status < 0)
	{
		LLOGLN(0, ("transport_check_fds: transport_read_nonblocking failed"));
		return status;
	}

	pos = stream_get_pos(transport->recv_buffer);
	if (pos > 0)
	{
		stream_set_pos(transport->recv_buffer, 0);
		if (transport->layer == TRANSPORT_LAYER_TSG)
		{
			if (pos <= 10)
			{
				stream_set_pos(transport->recv_buffer, pos);
				return 0;
			}
			stream_set_pos(transport->recv_buffer, 8);
			stream_read_uint16(transport->recv_buffer, length);
			stream_set_pos(transport->recv_buffer, 0);
			LLOGLN(10, ("transport_check_fds: got header tsg packet length %d", length));
			LLOGLN(10, ("transport_check_fds: dumping 1st 10 bytes of HTTP data"));
			LHEXDUMP(10, (transport->recv_buffer->data, 10));
		}
		else
		{
			/* Ensure header is available. */
			if (pos <= 4)
			{
				stream_set_pos(transport->recv_buffer, pos);
				return 0;
			}
			if (tpkt_verify_header(transport->recv_buffer)) /* TPKT */
			{
				length = tpkt_read_header(transport->recv_buffer);
			}
			else /* Fast Path */
			{
				length = fastpath_read_header(NULL, transport->recv_buffer);
			}
		}

		if (length == 0)
		{
			printf("transport_check_fds: protocol error, not a TPKT or Fast Path header.\n");
			freerdp_hexdump(stream_get_head(transport->recv_buffer), pos);
			return -1;
		}

		if (pos < length)
		{
			stream_set_pos(transport->recv_buffer, pos);
			return 0; /* Packet is not yet completely received. */
		}

		/* whole PDU is read in, for tsg, this is just the fragment */
		LLOGLN(10, ("transport_check_fds: got whole transport pdu"));

		proc_s = NULL;
		if (transport->layer == TRANSPORT_LAYER_TSG)
		{

			stream_set_pos(transport->recv_buffer, 0);
			if (tsg_skip_pdu(transport->tsg, transport->recv_buffer))
			{
				LLOGLN(10, ("transport_check_fds: tsg_skip_pdu returned true"));
				stream_set_pos(transport->recv_buffer, 0);
				return 0;
			}

			stream_set_pos(transport->recv_buffer, 0);
			stream_seek(transport->recv_buffer, 2);
			stream_read_uint8(transport->recv_buffer, ptype);
			stream_read_uint8(transport->recv_buffer, pfc_flags);
			stream_seek(transport->recv_buffer, 4);
			stream_read_uint16(transport->recv_buffer, frag_length);
			stream_read_uint16(transport->recv_buffer, auth_length);
			stream_read_uint32(transport->recv_buffer, call_id);
			stream_read_uint32(transport->recv_buffer, alloc_hint);
			auth_pad_length = transport->recv_buffer->data[frag_length - auth_length - 6];
			length = frag_length - auth_length - 24 - 8 - auth_pad_length;

			LLOGLN(10, ("transport_check_fds: ptype %d pfc_flags %d "
					"frag_length %d auth_length %d call_id %d alloc_hint %d auth_pad_length %d length %d",
					ptype, pfc_flags, frag_length, auth_length, call_id, alloc_hint, auth_pad_length, length));

			s = transport->proc_buffer;
			memcpy(s->p, transport->recv_buffer->data + 24, length);
			stream_seek(s, length);

			while (true) /* can be more than one RDP PDU in one TSG PDU */
			{
				pos = stream_get_pos(s);
				if (pos > 3)
				{
					rdp_pdu_length = get_rdp_pdu_length(s->data);
					LLOGLN(10, ("transport_check_fds: rdp_pdu_length %d pos %d", rdp_pdu_length, pos));
					if (pos >= rdp_pdu_length)
					{
						LLOGLN(10, ("transport_check_fds: got whole rdp pdu"));
						stream_set_pos(s, rdp_pdu_length);
						stream_seal(s);
						stream_set_pos(s, 0);
						if (do_callback(transport, s) != 0)
						{
							LLOGLN(0, ("transport_check_fds: do_callback failed"));
							return -1;
						}
						stream_set_pos(s, 0);
						extra_bytes = pos - rdp_pdu_length;
						LLOGLN(10, ("transport_check_fds: extra_bytes %d", extra_bytes));
						LHEXDUMP(10, (s->data + rdp_pdu_length - extra_bytes, extra_bytes));
						memmove(s->p, s->data + rdp_pdu_length, extra_bytes);
						stream_seek(s, extra_bytes);
						continue;
					}
				}
				break;
			}
		}
		else
		{
			proc_s = transport->recv_buffer;
			stream_set_pos(transport->recv_buffer, length);
			stream_seal(transport->recv_buffer);
			stream_set_pos(transport->recv_buffer, 0);
		}

		if (proc_s != NULL)
		{
			if (do_callback(transport, proc_s) != 0)
			{
				LLOGLN(0, ("transport_check_fds: do_callback failed"));
				return -1;
			}
		}

		stream_set_pos(transport->recv_buffer, 0);

		if (status < 0)
		{
			LLOGLN(0, ("transport_check_fds: failed"));
			return status;
		}
	}

	return 0;
}

tbool transport_set_blocking_mode(rdpTransport* transport, tbool blocking)
{
	transport->blocking = blocking;
	if (transport->settings->tsg)
		tcp_set_blocking_mode(transport->tcp_in, blocking);
	return tcp_set_blocking_mode(transport->tcp_out, blocking);
}

rdpTransport* transport_new(rdpSettings* settings)
{
	rdpTransport* transport;

	transport = (rdpTransport*) xzalloc(sizeof(rdpTransport));

	if (transport != NULL)
	{
		transport->tcp_in = tcp_new(settings);
		transport->settings = settings;

		/* a small 0.1ms delay when transport is blocking. */
		transport->usleep_interval = 100;

		/* receive buffer for non-blocking read. */
		transport->recv_buffer = stream_new(BUFFER_SIZE);

		/* for tsg fragmenting */
		transport->proc_buffer = stream_new(BUFFER_SIZE);

		/* buffers for blocking read/write */
		transport->recv_stream = stream_new(BUFFER_SIZE);
		transport->send_stream = stream_new(BUFFER_SIZE);

		transport->blocking = true;

		transport->layer = TRANSPORT_LAYER_TCP;
	}

	return transport;
}

void transport_free(rdpTransport* transport)
{
	if (transport != NULL)
	{
		stream_free(transport->recv_buffer);
		stream_free(transport->recv_stream);
		stream_free(transport->send_stream);
		stream_free(transport->proc_buffer);
		if (transport->tls_in)
		{
			tls_free(transport->tls_in);
			if (transport->layer == TRANSPORT_LAYER_TSG)
				tls_free(transport->tls_out);
		}
		tcp_free(transport->tcp_in);
		if (transport->layer == TRANSPORT_LAYER_TSG)
			tcp_free(transport->tcp_out);
		xfree(transport);
	}
}

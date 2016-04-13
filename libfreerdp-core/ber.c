/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Basic Encoding Rules (BER)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "ber.h"

int ber_read_length(STREAM* s, int* length)
{
	uint8 byte;

	if (stream_get_left(s) < 1)
	{
		return 0;
	}
	stream_read_uint8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (stream_get_left(s) < byte)
		{
			return 0;
		}

		if (byte == 1)
		{
			stream_read_uint8(s, *length);
		}
		else if (byte == 2)
		{
			stream_read_uint16_be(s, *length);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		*length = byte;
	}

	return 1;
}

/**
 * Write BER length.
 * @param s stream
 * @param length length
 */

int ber_write_length(STREAM* s, int length)
{
	if (length > 0xFF)
	{
		stream_write_uint8(s, 0x80 ^ 2);
		stream_write_uint16_be(s, length);
		return 3;
	}
	if (length > 0x7F)
	{
		stream_write_uint8(s, 0x80 ^ 1);
		stream_write_uint8(s, length);
		return 2;
	}
	stream_write_uint8(s, length);
	return 1;
}

int _ber_sizeof_length(int length)
{
	if (length > 0xFF)
		return 3;
	if (length > 0x7F)
		return 2;
	return 1;
}

//int ber_get_content_length(int length)
//{
//	if (length - 1 > 0x7F)
//		return length - 4;
//	else
//		return length - 2;
//}

/**
 * Read BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @return
 */

int ber_read_universal_tag(STREAM* s, uint8 tag, tbool pc)
{
	uint8 byte;

	if (stream_get_left(s) < 1)
	{
		return 0;
	}
	stream_read_uint8(s, byte);


	if (byte != (BER_CLASS_UNIV | BER_PC(pc) | (BER_TAG_MASK & tag)))
		return false;

	return true;
}

/**
 * Write BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @param pc primitive (false) or constructed (true)
 */

int ber_write_universal_tag(STREAM* s, uint8 tag, tbool pc)
{
	stream_write_uint8(s, (BER_CLASS_UNIV | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return 1;
}

/**
 * Read BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

tbool ber_read_application_tag(STREAM* s, uint8 tag, int* length)
{
	uint8 byte;

	if (tag > 30)
	{
		if (stream_get_left(s) < 1)
		{
			return 0;
		}
		stream_read_uint8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK))
			return false;

		if (stream_get_left(s) < 1)
		{
			return 0;
		}
		stream_read_uint8(s, byte);

		if (byte != tag)
			return false;

		ber_read_length(s, length);
	}
	else
	{
		if (stream_get_left(s) < 1)
		{
			return 0;
		}
		stream_read_uint8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag)))
			return false;

		ber_read_length(s, length);
	}

	return true;
}

/**
 * Write BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

void ber_write_application_tag(STREAM* s, uint8 tag, int length)
{
	if (tag > 30)
	{
		stream_write_uint8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);
		stream_write_uint8(s, tag);
		ber_write_length(s, length);
	}
	else
	{
		stream_write_uint8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));
		ber_write_length(s, length);
	}
}

tbool ber_read_contextual_tag(STREAM* s, uint8 tag, int* length, tbool pc)
{
	uint8 byte;

	if (stream_get_left(s) < 1)
	{
		return 0;
	}
	stream_read_uint8(s, byte);

	if (byte != ((BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag)))
	{
		stream_rewind(s, 1);
		return false;
	}

	return ber_read_length(s, length);
}

int ber_write_contextual_tag(STREAM* s, uint8 tag, int length, tbool pc)
{
	stream_write_uint8(s, (BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return 1 + ber_write_length(s, length);
}

int ber_sizeof_contextual_tag(int length)
{
	return 1 + _ber_sizeof_length(length);
}

tbool ber_read_sequence_tag(STREAM* s, int* length)
{
	uint8 byte;

	if (stream_get_left(s) < 1)
	{
		return 0;
	}
	stream_read_uint8(s, byte);

	if (byte != ((BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_SEQUENCE_OF)))
		return false;

	return ber_read_length(s, length);
}

/**
 * Write BER SEQUENCE tag.
 * @param s stream
 * @param length length
 */

int ber_write_sequence_tag(STREAM* s, int length)
{
	stream_write_uint8(s, (BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_MASK & BER_TAG_SEQUENCE));
	return 1 + ber_write_length(s, length);
}

int ber_sizeof_sequence(int length)
{
	return 1 + _ber_sizeof_length(length) + length;
}

int ber_sizeof_sequence_tag(int length)
{
	return 1 + _ber_sizeof_length(length);
}

tbool ber_read_enumerated(STREAM* s, uint8* enumerated, uint8 count)
{
	int length;

	if (!ber_read_universal_tag(s, BER_TAG_ENUMERATED, false) ||
			!ber_read_length(s, &length))
	{
		return false;
	}


	if (length != 1 || stream_get_left(s) < 1)
	{
		return false;
	}

	stream_read_uint8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return false;

	return true;
}

void ber_write_enumerated(STREAM* s, uint8 enumerated, uint8 count)
{
	ber_write_universal_tag(s, BER_TAG_ENUMERATED, false);
	ber_write_length(s, 1);
	stream_write_uint8(s, enumerated);
}

tbool ber_read_bit_string(STREAM* s, int* length, uint8* padding)
{
	if (!ber_read_universal_tag(s, BER_TAG_BIT_STRING, false) ||
			!ber_read_length(s, length))
	{
		return false;
	}

	if (stream_get_left(s) < 1)
	{
		return false;
	}

	stream_read_uint8(s, *padding);

	return true;
}

tbool ber_read_octet_string(STREAM* s, int* length)
{
	ber_read_universal_tag(s, BER_TAG_OCTET_STRING, false);
	ber_read_length(s, length);

	return true;
}

/**
 * Write a BER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

int ber_write_octet_string(STREAM* s, const uint8* oct_str, int length)
{
	int size = 0;

	size += ber_write_universal_tag(s, BER_TAG_OCTET_STRING, false);
	size += ber_write_length(s, length);
	stream_write(s, oct_str, length);
	size += length;
	return size;

}

tbool ber_read_octet_string_tag(STREAM* s, int* length)
{
	return
		ber_read_universal_tag(s, BER_TAG_OCTET_STRING, false) &&
		ber_read_length(s, length);
}


int ber_write_octet_string_tag(STREAM* s, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, false);
	ber_write_length(s, length);
	return 1 + _ber_sizeof_length(length);
}

int ber_sizeof_octet_string(int length)
{
	return 1 + _ber_sizeof_length(length) + length;
}

/**
 * Read a BER BOOLEAN
 * @param s
 * @param value
 */

tbool ber_read_boolean(STREAM* s, tbool* value)
{
	int length;
	uint8 v;

	if (!ber_read_universal_tag(s, BER_TAG_BOOLEAN, false) ||
			!ber_read_length(s, &length))
		return false;

	if (length != 1 || stream_get_left(s) < 1)
		return false;

	stream_read_uint8(s, v);
	*value = (v ? true : false);

	return true;
}

/**
 * Write a BER BOOLEAN
 * @param s
 * @param value
 */

void ber_write_boolean(STREAM* s, tbool value)
{
	ber_write_universal_tag(s, BER_TAG_BOOLEAN, false);
	ber_write_length(s, 1);
	stream_write_uint8(s, value ? 0xFF : 0);
}

tbool ber_read_integer(STREAM* s, uint32* value)
{
	int length;

	if (!ber_read_universal_tag(s, BER_TAG_INTEGER, false) ||
			!ber_read_length(s, &length) ||
			stream_get_left(s) < length)
	{
		return false;
	}

	if (value == NULL)
	{
		if (stream_get_left(s) < length)
		{
			return false;
		}
		stream_seek(s, length);
		return true;
	}

	if (length == 1)
	{
		stream_read_uint8(s, *value);
	}
	else if (length == 2)
	{
		stream_read_uint16_be(s, *value);
	}
	else if (length == 3)
	{
		uint8 byte;
		stream_read_uint8(s, byte);
		stream_read_uint16_be(s, *value);
		*value += (byte << 16);
	}
	else if (length == 4)
	{
		stream_read_uint32_be(s, *value);
	}
	else if (length == 8)
	{
		fprintf(stderr, "%s: should implement reading an 8 bytes integer\n", __FUNCTION__);
		return false;
	}
	else
	{
		fprintf(stderr, "%s: should implement reading an integer with length=%d\n", __FUNCTION__, length);
		return false;
	}

	return true;
}

/**
 * Write a BER INTEGER
 * @param s
 * @param value
 */

int ber_write_integer(STREAM* s, uint32 value)
{

	if (value < 0x80)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, false);
		ber_write_length(s, 1);
		stream_write_uint8(s, value);
		return 3;
	}
	else if (value < 0x8000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, false);
		ber_write_length(s, 2);
		stream_write_uint16_be(s, value);
		return 4;
	}
	else if (value < 0x800000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, false);
		ber_write_length(s, 3);
		stream_write_uint8(s, (value >> 16));
		stream_write_uint16_be(s, (value & 0xFFFF));
		return 5;
	}
	else if (value < 0x80000000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, false);
		ber_write_length(s, 4);
		stream_write_uint32_be(s, value);
		return 6;
	}

	return 0;
}

int ber_sizeof_integer(uint32 value)
{
	if (value < 0x80)
	{
		return 3;
	}
	else if (value < 0x8000)
	{
		return 4;
	}
	else if (value < 0x800000)
	{
		return 5;
	}
	else if (value < 0x80000000)
	{
		return 6;
	}

	return 0;
}

tbool ber_read_integer_length(STREAM* s, int* length)
{
	return
		ber_read_universal_tag(s, BER_TAG_INTEGER, false) &&
		ber_read_length(s, length);
}

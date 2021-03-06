/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "platform.hpp"
#include "uuid.hpp"
#include "err.hpp"

#if defined ZMQ_HAVE_WINDOWS

zmq::uuid_t::uuid_t ()
{
    RPC_STATUS ret = UuidCreate (&uuid);
    zmq_assert (ret == RPC_S_OK);
    ret = UuidToString (&uuid, &uuid_str);
    zmq_assert (ret == RPC_S_OK);

	/*
	HRESULT hr = CoCreateGUID (&uuid);
	zmq_assert (hr == S_OK);
	int rc = StringFromGUID2 (uuid, uuid_str, 40);
	zmq_assert (rc != 0);
	*/
}

zmq::uuid_t::~uuid_t ()
{
}

const char *zmq::uuid_t::to_string ()
{
    return (char*) uuid_str;
}

#elif defined ZMQ_HAVE_FREEBSD

#include <stdlib.h>
#include <uuid.h>

zmq::uuid_t::uuid_t ()
{
    uint32_t status;
    uuid_create (&uuid, &status);
    zmq_assert (status == uuid_s_ok);
    uuid_to_string (&uuid, &uuid_str, &status);
    zmq_assert (status == uuid_s_ok);
}

zmq::uuid_t::~uuid_t ()
{
    free (uuid_str);
}

const char *zmq::uuid_t::to_string ()
{
    return uuid_str;
}

#elif defined ZMQ_HAVE_LINUX || defined ZMQ_HAVE_SOLARIS || defined ZMQ_HAVE_OSX

#include <uuid/uuid.h>

zmq::uuid_t::uuid_t ()
{
    uuid_generate (uuid);
    uuid_unparse (uuid, uuid_buf);
}

zmq::uuid_t::~uuid_t ()
{
}

const char *zmq::uuid_t::to_string ()
{
    return uuid_buf;
}

#else

#include <stdio.h>
#include <string.h>
#include <openssl/rand.h>

zmq::uuid_t::uuid_t ()
{
    unsigned char rand_buf [16];
    int ret = RAND_bytes (rand_buf, sizeof rand_buf);
    zmq_assert (ret == 1);

    //  Read in UUID fields.
    memcpy (&time_low, rand_buf, sizeof time_low);
    memcpy (&time_mid, rand_buf + 4, sizeof time_mid);
    memcpy (&time_hi_and_version, rand_buf + 6, sizeof time_hi_and_version);
    memcpy (&clock_seq_hi_and_reserved, rand_buf + 8,
        sizeof clock_seq_hi_and_reserved);
    memcpy (&clock_seq_low, rand_buf + 9, sizeof clock_seq_low);
    memcpy (&node [0], rand_buf + 10, sizeof node);

    //  Store UUID version number.
    time_hi_and_version = (time_hi_and_version & 0x0fff) | 4 << 12;

    //  Store UUID type.
    clock_seq_hi_and_reserved = (clock_seq_hi_and_reserved & 0x3f) | 0x80;

    snprintf (uuid_buf, sizeof uuid_buf,
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        time_low,
        time_mid,
        time_hi_and_version,
        clock_seq_hi_and_reserved,
        clock_seq_low,
        node [0], node [1], node [2], node [3], node [4], node [5]);
}

zmq::uuid_t::~uuid_t ()
{
}

const char *zmq::uuid_t::to_string ()
{
    return uuid_buf;
}

#endif

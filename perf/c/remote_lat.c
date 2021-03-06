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

#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main (int argc, char *argv [])
{
    const char *connect_to;
    int roundtrip_count;
    int message_size;
    void *ctx;
    void *s;
    int rc;
    int i;
    zmq_msg_t msg;
    void *watch;
    unsigned long elapsed;
    double latency;

    if (argc != 4) {
        printf ("usage: remote_lat <connect-to> <message-size> "
            "<roundtrip-count>\n");
        return 1;
    }
    connect_to = argv [1];
    message_size = atoi (argv [2]);
    roundtrip_count = atoi (argv [3]);

    ctx = zmq_init (1, 1, 0);
    assert (ctx);

    s = zmq_socket (ctx, ZMQ_REQ);
    assert (s);

    rc = zmq_connect (s, connect_to);
    assert (rc == 0);

    rc = zmq_msg_init_size (&msg, message_size);
    assert (rc == 0);
    memset (zmq_msg_data (&msg), 0, message_size);

    watch = zmq_stopwatch_start ();

    for (i = 0; i != roundtrip_count; i++) {
        rc = zmq_send (s, &msg, 0);
        assert (rc == 0);
        rc = zmq_recv (s, &msg, 0);
        assert (rc == 0);
        assert (zmq_msg_size (&msg) == message_size);
    }

    elapsed = zmq_stopwatch_stop (watch);

    rc = zmq_msg_close (&msg);
    assert (rc == 0);

    latency = (double) elapsed / (roundtrip_count * 2);

    printf ("message size: %d [B]\n", (int) message_size);
    printf ("roundtrip count: %d\n", (int) roundtrip_count);
    printf ("average latency: %.3f [us]\n", (double) latency);

    rc = zmq_close (s);
    assert (rc == 0);

    rc = zmq_term (ctx);
    assert (rc == 0);

    return 0;
}

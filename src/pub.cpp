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

#include "../bindings/c/zmq.h"

#include "pub.hpp"
#include "err.hpp"
#include "msg_content.hpp"
#include "pipe.hpp"

zmq::pub_t::pub_t (class app_thread_t *parent_) :
    socket_base_t (parent_)
{
    options.requires_in = false;
    options.requires_out = true;
}

zmq::pub_t::~pub_t ()
{
    for (out_pipes_t::size_type i = 0; i != out_pipes.size (); i++)
        out_pipes [i]->term ();
    out_pipes.clear ();
}

void zmq::pub_t::xattach_pipes (class reader_t *inpipe_,
    class writer_t *outpipe_)
{
    zmq_assert (!inpipe_);
    out_pipes.push_back (outpipe_);
}

void zmq::pub_t::xdetach_inpipe (class reader_t *pipe_)
{
    zmq_assert (false);
}

void zmq::pub_t::xdetach_outpipe (class writer_t *pipe_)
{
    out_pipes.erase (pipe_);
}

void zmq::pub_t::xkill (class reader_t *pipe_)
{
    zmq_assert (false);
}

void zmq::pub_t::xrevive (class reader_t *pipe_)
{
    zmq_assert (false);
}

int zmq::pub_t::xsetsockopt (int option_, const void *optval_,
    size_t optvallen_)
{
    errno = EINVAL;
    return -1;
}

int zmq::pub_t::xsend (zmq_msg_t *msg_, int flags_)
{
    out_pipes_t::size_type pipes_count = out_pipes.size ();

    //  If there are no pipes available, simply drop the message.
    if (pipes_count == 0) {
        int rc = zmq_msg_close (msg_);
        zmq_assert (rc == 0);
        rc = zmq_msg_init (msg_);
        zmq_assert (rc == 0);
        return 0;
    }

    //  First check whether all pipes are available for writing.
    for (out_pipes_t::size_type i = 0; i != pipes_count; i++)
        if (!out_pipes [i]->check_write (zmq_msg_size (msg_))) {
            errno = EAGAIN;
            return -1;
        }

    msg_content_t *content = (msg_content_t*) msg_->content;

    //  For VSMs the copying is straighforward.
    if (content == (msg_content_t*) ZMQ_VSM) {
        for (out_pipes_t::size_type i = 0; i != pipes_count; i++) {
            out_pipes [i]->write (msg_);
            if (!(flags_ & ZMQ_NOFLUSH))
                out_pipes [i]->flush ();
        }
        int rc = zmq_msg_init (msg_);
        zmq_assert (rc == 0);
        return 0;
    }

    //  Optimisation for the case when there's only a single pipe
    //  to send the message to - no refcount adjustment i.e. no atomic
    //  operations are needed.
    if (pipes_count == 1) {
        out_pipes [0]->write (msg_);
        if (!(flags_ & ZMQ_NOFLUSH))
            out_pipes [0]->flush ();
        int rc = zmq_msg_init (msg_);
        zmq_assert (rc == 0);
        return 0;
    }

    //  There are at least 2 destinations for the message. That means we have
    //  to deal with reference counting. First add N-1 references to
    //  the content (we are holding one reference anyway, that's why -1).
    if (msg_->shared)
        content->refcnt.add (pipes_count - 1);
    else {
        content->refcnt.set (pipes_count);
        msg_->shared = true;
    }

    //  Push the message to all destinations.
    for (out_pipes_t::size_type i = 0; i != pipes_count; i++) {
        out_pipes [i]->write (msg_);
        if (!(flags_ & ZMQ_NOFLUSH))
            out_pipes [i]->flush ();
    }

    //  Detach the original message from the data buffer.
    int rc = zmq_msg_init (msg_);
    zmq_assert (rc == 0);

    return 0;
}

int zmq::pub_t::xflush ()
{
    out_pipes_t::size_type pipe_count = out_pipes.size ();
    for (out_pipes_t::size_type i = 0; i != pipe_count; i++)
        out_pipes [i]->flush ();
    return 0;
}

int zmq::pub_t::xrecv (zmq_msg_t *msg_, int flags_)
{
    errno = ENOTSUP;
    return -1;
}

bool zmq::pub_t::xhas_in ()
{
    return false;
}

bool zmq::pub_t::xhas_out ()
{
    //  TODO: Reimplement when queue limits are added.
    return true;
}


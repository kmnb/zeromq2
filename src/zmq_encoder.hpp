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

#ifndef __ZMQ_ZMQ_ENCODER_HPP_INCLUDED__
#define __ZMQ_ZMQ_ENCODER_HPP_INCLUDED__

#include "../bindings/c/zmq.h"

#include "encoder.hpp"

namespace zmq
{
    //  Encoder for 0MQ backend protocol. Converts messages into data batches.

    class zmq_encoder_t : public encoder_t <zmq_encoder_t>
    {
    public:

        zmq_encoder_t (size_t bufsize_, bool trim_prefix_);
        ~zmq_encoder_t ();

        void set_inout (struct i_inout *source_);

    private:

        bool size_ready ();
        bool message_ready ();

        struct i_inout *source;
        ::zmq_msg_t in_progress;
        unsigned char tmpbuf [9];

        bool trim_prefix;

        zmq_encoder_t (const zmq_encoder_t&);
        void operator = (const zmq_encoder_t&);
    };
}

#endif


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

#include <string.h>

#include "tcp_listener.hpp"
#include "platform.hpp"
#include "ip.hpp"
#include "config.hpp"
#include "err.hpp"

#ifdef ZMQ_HAVE_WINDOWS

zmq::tcp_listener_t::tcp_listener_t () :
    s (retired_fd)
{
    memset (&addr, 0, sizeof (addr));
}

zmq::tcp_listener_t::~tcp_listener_t ()
{
    if (s != retired_fd)
        close ();
}

int zmq::tcp_listener_t::set_address (const char *addr_)
{
    //  Convert the interface into sockaddr_in structure.
    int rc = resolve_ip_interface (&addr, addr_);
    if (rc != 0)
        return rc;

    //  Create a listening socket.
    s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        wsa_error_to_errno ();
        return -1;
    }

    //  Allow reusing of the address.
    int flag = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_REUSEADDR,
        (const char*) &flag, sizeof (int));
    wsa_assert (rc != SOCKET_ERROR);

    //  Set the non-blocking flag.
    flag = 1;
    rc = ioctlsocket (s, FIONBIO, (u_long*) &flag);
    wsa_assert (rc != SOCKET_ERROR);

    //  Bind the socket to the network interface and port.
    rc = bind (s, (struct sockaddr*) &addr, sizeof (addr));
    if (rc == SOCKET_ERROR) {
        wsa_error_to_errno ();
        return -1;
    }

    //  Listen for incomming connections.
    rc = listen (s, 1);
    if (rc == SOCKET_ERROR) {
        wsa_error_to_errno ();
        return -1;
    }

    return 0;
}

int zmq::tcp_listener_t::close ()
{
    zmq_assert (s != retired_fd);
    int rc = closesocket (s);
    wsa_assert (rc != SOCKET_ERROR);
    s = retired_fd;
    return 0;
}

zmq::fd_t zmq::tcp_listener_t::get_fd ()
{
    return s;
}

zmq::fd_t zmq::tcp_listener_t::accept ()
{
    zmq_assert (s != retired_fd);

    //  Accept one incoming connection.
    fd_t sock = ::accept (s, NULL, NULL);
    if (sock == INVALID_SOCKET && 
          (WSAGetLastError () == WSAEWOULDBLOCK ||
          WSAGetLastError () == WSAECONNRESET))
        return retired_fd;

    zmq_assert (sock != INVALID_SOCKET);

    // Set to non-blocking mode.
    unsigned long argp = 1;
    int rc = ioctlsocket (sock, FIONBIO, &argp);
    wsa_assert (rc != SOCKET_ERROR);

    //  Disable Nagle's algorithm.
    int flag = 1;
    rc = setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
        sizeof (int));
    wsa_assert (rc != SOCKET_ERROR);

    return sock;
}

#else

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

zmq::tcp_listener_t::tcp_listener_t () :
    s (retired_fd)
{
    memset (&addr, 0, sizeof (addr));
}

zmq::tcp_listener_t::~tcp_listener_t ()
{
    if (s != retired_fd)
        close ();
}

int zmq::tcp_listener_t::set_address (const char *addr_)
{
    //  Convert the interface into sockaddr_in structure.
    int rc = resolve_ip_interface (&addr, addr_);
    if (rc != 0)
        return rc;

    //  Create a listening socket.
    s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1)
        return -1;

    //  Allow reusing of the address.
    int flag = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (int));
    errno_assert (rc == 0);

    //  Set the non-blocking flag.
    flag = fcntl (s, F_GETFL, 0);
    if (flag == -1) 
        flag = 0;
    rc = fcntl (s, F_SETFL, flag | O_NONBLOCK);
    errno_assert (rc != -1);

    //  Bind the socket to the network interface and port.
    rc = bind (s, (struct sockaddr*) &addr, sizeof (addr));
    if (rc != 0) {
        close ();
        return -1;
    }
              
    //  Listen for incomming connections.
    rc = listen (s, tcp_connection_backlog);
    if (rc != 0) {
        close ();
        return -1;
    }

    return 0;
}

int zmq::tcp_listener_t::close ()
{
    zmq_assert (s != retired_fd);
    int rc = ::close (s);
    if (rc != 0)
        return -1;
    s = retired_fd;
    return 0;
}

zmq::fd_t zmq::tcp_listener_t::get_fd ()
{
    return s;
}

zmq::fd_t zmq::tcp_listener_t::accept ()
{
    zmq_assert (s != retired_fd);

    //  Accept one incoming connection.
    fd_t sock = ::accept (s, NULL, NULL);

#if (defined ZMQ_HAVE_LINUX || defined ZMQ_HAVE_FREEBSD || \
     defined ZMQ_HAVE_OPENBSD || defined ZMQ_HAVE_OSX || \
     defined ZMQ_HAVE_OPENVMS)
    if (sock == -1 && 
        (errno == EAGAIN || errno == EWOULDBLOCK || 
         errno == EINTR || errno == ECONNABORTED))
        return retired_fd;
#elif (defined ZMQ_HAVE_SOLARIS || defined ZMQ_HAVE_AIX)
    if (sock == -1 && 
        (errno == EWOULDBLOCK || errno == EINTR || 
         errno == ECONNABORTED || errno == EPROTO))
        return retired_fd;
#elif defined ZMQ_HAVE_HPUX
    if (sock == -1 && 
        (errno == EAGAIN || errno == EWOULDBLOCK || 
         errno == EINTR || errno == ECONNABORTED || errno == ENOBUFS))
        return retired_fd;
#elif defined ZMQ_HAVE_QNXNTO 
    if (sock == -1 && 
        (errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED))
        return retired_fd;
#endif

    errno_assert (sock != -1); 

    // Set to non-blocking mode.
    int flags = fcntl (s, F_GETFL, 0);
    if (flags == -1) 
        flags = 0;
    int rc = fcntl (sock, F_SETFL, flags | O_NONBLOCK);
    errno_assert (rc != -1);

    //  Disable Nagle's algorithm.
    int flag = 1;
    rc = setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
        sizeof (int));
    errno_assert (rc == 0);

#ifdef ZMQ_HAVE_OPENVMS
    //  Disable delayed acknowledgements.
    flag = 1;
    rc = setsockopt (sock, IPPROTO_TCP, TCP_NODELACK, (char*) &flag,
        sizeof (int));
    errno_assert (rc != SOCKET_ERROR);
#endif

    return sock;
}

#endif

//  Copyright (C) 1999 Silicon Graphics, Inc.  All Rights Reserved.
//  
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of version 2 of the GNU General Public License as
//  published by the Free Software Foundation.
//
//  This program is distributed in the hope that it would be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Further, any
//  license provided herein, whether implied or otherwise, is limited to
//  this program in accordance with the express provisions of the GNU
//  General Public License.  Patent licenses, if any, provided herein do not
//  apply to combinations of this program with other product or programs, or
//  any other product whatsoever.  This program is distributed without any
//  warranty that the program is delivered free of the rightful claim of any
//  third person by way of infringement or the like.  See the GNU General
//  Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write the Free Software Foundation, Inc., 59
//  Temple Place - Suite 330, Boston MA 02111-1307, USA.

#include "NetConnection.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#include "Log.h"
#include "Scheduler.h"

NetConnection::NetConnection(int a_fd,
			     UnblockHandler uhandler, void *uclosure)
    : fd(a_fd),
      iready(true), oready(true),
      iend(ibuf + sizeof(ibuf)),
      itail(ibuf),
      unblock_handler(uhandler), closure(uclosure)
{
    // It must be the case that Length is a 32 bit int.
    assert (sizeof(Length) == 4);
    
    omsgList = omsgListTail = NULL;
    
    // Enable nonblocking output on socket.

    int yes = 1;
    if (ioctl(fd, FIONBIO, &yes) < 0)
    {   Log::perror("can't set NBIO on fd %d", fd);
	shutdown(false);
	return;
    }

#ifdef TINY_BUFFERS			/* This kills throughput. */

    // Reduce kernel's send and receive buffer sizes.  Useful for debugging
    // flow control.

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &osize, sizeof osize))
	Log::perror("setsockopt(%d, SO_SNDBUF)", fd);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &isize, sizeof isize))
	Log::perror("setsockopt(%d, SO_RCVBUF)", fd);

#endif /* !TINY_BUFFERS */

    // Wait for something to read.

    (void) Scheduler::install_read_handler(fd, read_handler, this);
}

NetConnection::~NetConnection()
{
    shutdown(false);
}

void
NetConnection::shutdown(bool call_input)
{
    if (fd >= 0)
    {
        Log::info("Shutting down connection");
	Scheduler::IOHandler oldh;
	if (iready && oready)
	{   oldh = Scheduler::remove_read_handler(fd);
	    assert(oldh == read_handler);
	}
	if (!oready)
	{   oldh = Scheduler::remove_write_handler(fd);
	    assert(oldh == write_handler);
	}
	(void) close(fd);
	fd = -1;
	oready = true;
	if (call_input)
	    input_msg(NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Input

void
NetConnection::read_handler(int fd, void *closure)
{
    NetConnection *connection = (NetConnection *) closure;
    assert(fd == connection->fd);
    connection->input();
}

void
NetConnection::input()
{
    if (fd < 0)
	return;

    // Read from socket.
    
    assert(itail < iend);
    int maxbytes = iend - itail;
    int ret = recv(fd, itail, maxbytes, 0);
    if (ret < 0)
    {   if (errno != EAGAIN && errno != ECONNRESET)
	{   Log::perror("fd %d read error", fd);
	    shutdown();
	}
	return;
    }
    else if (ret == 0)
    {   shutdown();
	return;
    }
    else // (ret > 0)
    {
	itail += ret;
    }

    deliver_input();
}

void
NetConnection::deliver_input()
{
    // Find messages and process them.

    char *ihead = ibuf;
    while (iready && oready && ihead + sizeof (Length) <= itail)
    {
        Length len;
        memcpy(&len, ihead, sizeof(Length));
	len = ntohl(len);

	if (len > MAXMSGSIZE)
	{   Log::error("fd %d message length %d bytes exceeds max of %d.",
		       fd, len, MAXMSGSIZE);
	    shutdown();
	    return;
	}
	if (ihead + sizeof (Length) + len > itail)
	    break;

	// Call the message reader.

	if (input_msg(ihead + sizeof (Length), len) == false) {
            // if input_msg sees an error in the message and thinks
            // the connection should be closed, it will return false.
            shutdown();
            return;
        }

	ihead += sizeof (Length) + len;
    }

    // If data remain in buffer, slide them to the left.

    assert(ihead <= itail);
    int remaining = itail - ihead;
    if (remaining && ihead != ibuf)
	memmove(ibuf, ihead, remaining);
    itail = ibuf + remaining;
    assert(itail < iend);
}

bool
NetConnection::ready_for_output() const
{
    return oready;
}

void
NetConnection::ready_for_input(bool tf)
{
    set_handlers(tf, oready);
}

///////////////////////////////////////////////////////////////////////////////
//  Output

void
NetConnection::mprintf(const char *format, ...)
{
    if (fd < 0)
	return;				// if closed, do nothing.

    va_list args;
    va_start(args, format);

    msgList_t * msg = new msgList_t;
    msg->next = NULL;
    Length len = vsnprintf(
        msg->msg + 4, MAXMSGSIZE + 1, format, args) + 1;
    va_end(args);

    if (len <= 0 || len == MAXMSGSIZE+1) {
        Log::error("tried to write a message that was too big");
        assert(0);
        // protocol botch.  Don't send the message.
        return;
    }
    
    if (omsgListTail) {
        omsgListTail = omsgListTail->next = msg;
    } else {
        omsgList = omsgListTail = msg;
    }
    msg->len = 4 + len;
    
    len = htonl(len);
    memcpy(msg->msg, &len, 4);
    flush();
}

void
NetConnection::flush()
{
    while (omsgList) 
    {
	int ret = send(fd, omsgList->msg, omsgList->len, 0);
        if (ret < 0 && errno == EWOULDBLOCK) 
        {
            break;
        } else 
        {
            if (ret >= 0) 
            {
                assert(ret == omsgList->len);
            } else 
            {
                /* Since the client library can close it's fd before
                 * getting acks from all FAMCancelMonitor requests we
                 * may get a broken pipe error here when writing the ack.
                 * Don't threat this as an error, since that fills the logs
                 * with crap.
                 */
                if (errno == EPIPE)
                {
                    Log::debug("fd %d write error: %m", fd);
                } else
                {
                    Log::error("fd %d write error: %m", fd);
                }
            }
            msgList_t *oldHead = omsgList;
            omsgList = omsgList->next;
            if (omsgListTail == oldHead) {
                omsgListTail = NULL;
            }
            delete oldHead;
	}
    }
    set_handlers(iready, !omsgList);
}

void
NetConnection::set_handlers(bool new_iready, bool new_oready)
{
    Scheduler::IOHandler oldh;
    bool call_unblock = false;

    if (oready != new_oready)
    {   if (new_oready)
	{   oldh = Scheduler::remove_write_handler(fd);
	    assert(oldh == write_handler);
	    call_unblock = true;
	}
	else
	{   oldh = Scheduler::install_write_handler(fd, write_handler, this);
	    assert(oldh == NULL);
	}
    }

    //  Install read_handler iff iready && oready.

    if ((iready && oready) != (new_iready && new_oready))
    {
	if (new_iready && new_oready)
	{   oldh = Scheduler::install_read_handler(fd, read_handler, this);
	    assert(oldh == NULL);
	}
	else
	{   oldh = Scheduler::remove_read_handler(fd);
	    assert(oldh == read_handler);
	}
    }
    bool old_iready = iready;
    iready = new_iready;
    oready = new_oready;

    //  If we unblocked output, call the unblock handler.
    //  If there's input to deliver, deliver it.

    if (call_unblock && unblock_handler)
    {   assert(!iready || old_iready);
	(*unblock_handler)(closure);
    }
    else if (iready && !old_iready)
	deliver_input();
}

void
NetConnection::write_handler(int fd, void *closure)
{
    NetConnection *connection = (NetConnection *) closure;
    assert(connection->fd == fd);
    connection->flush();
}


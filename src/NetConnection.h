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

#ifndef NetConnection_included
#define NetConnection_included

#include <sys/types.h>
#include "Boolean.h"
#include <limits.h>

//  NetConnection is an abstract base class that implements an event
//  driven, flow controlled reliable datagram connection over an
//  already open stream socket.
//
//  The incoming and outgoing data streams are broken into messages.
//  The format of a message is four bytes of message length (MSB
//  first) followed by "length" bytes of data.  The last byte of data
//  should be NUL ('\0').
//
//  The mprintf() function outputs a message using printf-style
//  formatting.  It appends a NUL byte to the message and prepends the
//  message length.  It also automatically flushes the message.  If
//  the connection has closed, mprintf() returns immediately.
//
//  When a complete message is received, the pure virtual function
//  input_msg() is called with the length and the address of the
//  message.  If EOF is read on the connection, input_msg() is called
//  with a NULL address and count (analogous to read(2) returning 0
//  bytes).
//
//  NetConnection implements flow control.  Whenever the output buffer
//  is empty, read events are accepted from the Scheduler.  Whenever
//  the output buffer can't be flushed, reading is suspended, and the
//  Scheduler watches for the connection to be writable.  This means
//  that input is only accepted when it's possible to send a reply.
//  The unblock handler, if any, is called whenever output becomes
//  unblocked.

class NetConnection {

public:

    typedef void (*UnblockHandler)(void *closure);

    NetConnection(int fd, UnblockHandler, void *closure);

    virtual ~NetConnection();
    bool ready_for_output() const;
    void ready_for_input(bool);
    int get_fd() const { return fd; }

protected:

    virtual bool input_msg(const char *data, unsigned nbytes) = 0;
    void mprintf(const char *format, ...);

private:

    enum { MAXMSGSIZE = PATH_MAX + 40 };
    typedef u_int32_t Length;
    typedef struct msgList_s {
        char msg[MAXMSGSIZE+5];  //  + 4 for 32-bit length, + 1 for overflow
        int len;
        msgList_s * next;
    } msgList_t;
    
    msgList_t *omsgList;
    msgList_t *omsgListTail;

    int fd;
    bool iready, oready;
    char ibuf[MAXMSGSIZE+5];     //  + 4 for 32-bit length, + 1 for overflow
    char *iend;
    char *itail;
    UnblockHandler unblock_handler;
    void *closure;

    void set_handlers(bool new_iready, bool new_oready);
    void shutdown(bool = true);

    //  Input

    void input();
    void deliver_input();
    static void read_handler(int fd, void *closure);

    //  Output

    void flush();
    static void write_handler(int fd, void *closure);

    NetConnection(const NetConnection&); // Do not copy
    NetConnection& operator = (const NetConnection&);	 //  or assign.

};

#endif /* !NetConnection_included */

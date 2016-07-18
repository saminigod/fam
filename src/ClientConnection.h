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

#ifndef ClientConnection_included
#define ClientConnection_included

#include <sys/param.h>
#include <limits.h>
#include "NetConnection.h"
#include "Request.h"

class Event;

//  ClientConnection implements the fam server protocol.  It generates
//  fam events.  It does not parse fam requests because the API for its
//  caller would be really messy.  Instead it hands received messages
//  to its owner unparsed.
//
//  The field order is important -- the big net buffers are last.
//  Since the output buffer is twice as big as the input buffer,
//  it comes after the input buffer.

struct sockaddr_un;
class ClientConnection : public NetConnection {

public:

    typedef bool (*InputHandler)(const char *, unsigned nbytes, void *closure);

    ClientConnection(int fd, InputHandler, UnblockHandler, void *closure);

    void send_event(const Event&, Request, const char *name);
    void send_sockaddr_un(const sockaddr_un &sun);

protected:

    bool input_msg(const char *, unsigned);

private:
    InputHandler ihandler;
    void *iclosure;
};

#endif /* !ClientConnection_included */

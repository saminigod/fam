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

#include "ClientConnection.h"

#include <assert.h>
#include <sys/un.h>

#include "Event.h"

ClientConnection::ClientConnection(int fd,
				   InputHandler inhandler,
				   UnblockHandler unhandler, void *closure)
    : NetConnection(fd, unhandler, closure),
      ihandler(inhandler), iclosure(closure)
{ }

bool
ClientConnection::input_msg(const char *msg, unsigned nbytes)
{
    return (*ihandler)(msg, nbytes, iclosure);
}

void
ClientConnection::send_event(const Event& event, Request request,
			     const char *name)
{
    // Format message.
    // Have to send fake change event here, as previous versions of
    // fam (i.e. in IRIX 6.5.5) expect that change events will come with a
    // list of character flags saying what changed.  We'll use a 'c'
    // character, which means that the ctime changed.
    char code = event.code();
    if (event == Event::Changed)
	mprintf("%c%lu c %s\n", code, request , name);
    else
	mprintf("%c%lu %s\n", code, request, name);
}

void
ClientConnection::send_sockaddr_un(const sockaddr_un &sun)
{
    mprintf("%s", sun.sun_path);
}

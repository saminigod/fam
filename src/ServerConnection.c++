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

#include "ServerConnection.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "Cred.h"
#include "Event.h"
#include "Log.h"

ServerConnection::ServerConnection(int fd,
				   EventHandler eh, DisconnectHandler dh,
				   void *clos)
    : NetConnection(fd, NULL, NULL),
      event_handler(eh), disconnect_handler(dh), closure(clos)
{
    assert(fd >= 0);
    assert(eh);
    assert(dh);
}

bool
ServerConnection::input_msg(const char *msg, unsigned nbytes)
{

    // If there's an error parsing the message, then just forget about
    // the message and return true.  If we returned false, then the
    // connection would go down, but the remote fam would notice and
    // reconnect almost immediately, and send another bad message,
    // which would result in thrashing on both ends.
    
    if (msg == NULL)
    {   (*disconnect_handler)(closure);	// Disconnected.
	return true;
    }

    if (nbytes == 0 || msg[nbytes - 1])
    {   Log::debug("protocol error");
	return true;
    }

    char *p = (char *) msg;
    char opcode = *p++;
    Request request = strtol(p, &p, 10);
    // If the opcode is "c", then the next part of the msg contains a
    // list of character flags indicating what changed about the
    // file.  We currently ignore those flags.
    //
    if (opcode == 'c') 
    {
        p++;
        while (isascii(*p) && !isspace(*p))
            p++;
    }
    
    const Event *event = Event::getEventFromOpcode(opcode);

    // Skip over the space
    p++;
    
    char name[PATH_MAX + 1];
    int i;
    for (i = 0; *p; i++)
    {   if (i >= PATH_MAX)
	{   Log::error("path name too long (%d chars)", i);
	    return true;
	}
	name[i] = *p++;
    }
    if ((i > 0) && (name[i - 1] != '\n'))
    {
	Log::error("path name doesn't end in newline");
	return true;
    }
    name[i ? i - 1 : 0] = '\0';		// strip the trailing newline

    (*event_handler)(event, request, name, closure);
    return true;
}

void
ServerConnection::send_monitor(ClientInterest::Type code, Request request,
			       const char *path, const Cred& cr)
{
    
    const char * cred_buf = cr.getAddlGroupsString();
    if (cred_buf[0])
	mprintf("%c%d %d %d %s\n%c%s", code, request, cr.uid(), cr.gid(), path,
		'\0', cred_buf);
    else
	mprintf("%c%d %d %d %s\n", code, request, cr.uid(), cr.gid(), path);
}

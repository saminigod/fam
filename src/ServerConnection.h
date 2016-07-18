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

#ifndef ServerConnection_included
#define ServerConnection_included

#include <string.h>
#include <limits.h>
#include <sys/param.h>

#include "ClientInterest.h"
#include "NetConnection.h"
#include "Request.h"

class Cred;

//  A ServerConnection implements the client side of the fam protocol,
//  and it also handles network I/O. The various requests, monitor,
//  cancel, etc., can be sent.  When FAM events arrive, the
//  EventHandler is called.
//
//  ServerConnection inherits from NetConnection.  This means that
//  users of ServerConnection can test for I/O ready
//  (ready_for_input(), ready_for_output()) and that they need to
//  specify a DisconnectHandler which will be called when the server
//  goes away.

class ServerConnection : public NetConnection {

public:

    typedef void (*EventHandler)(const Event*, Request, const char *path,
				 void *closure);
    typedef void (*DisconnectHandler)(void *closure);

    ServerConnection(int fd, EventHandler, DisconnectHandler, void *closure);

    void send_monitor(ClientInterest::Type, Request,
		      const char *path, const Cred&);
    void send_cancel(Request r)		{ mprintf("C%d 0 0\n", r); }
    void send_suspend(Request r)	{ mprintf("S%d 0 0\n", r); }
    void send_resume(Request r)		{ mprintf("U%d 0 0\n", r); }
    void send_name(const char *n)	{ mprintf("N0 0 0 %s\n", n); }

protected:

    bool input_msg(const char *msg, unsigned nbytes);

private:

    EventHandler event_handler;
    DisconnectHandler disconnect_handler;
    void *closure;
};

#endif /* !ServerConnection_included */

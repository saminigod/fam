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

#ifndef RPC_TCP_Connector_included
#define RPC_TCP_Connector_included

#include <sys/types.h>
#include <netinet/in.h>

#include "Boolean.h"

//  An RPC_TCP_Connector is an object that connects to a server.
//  Specifically, a service registered with portmapper that uses
//  TCP/IP.
//
//  The destination host, program, and version are specified when the
//  R.T.Connector is created.
//
//  An R.T.Connector, when activated, tries repeatedly to connect,
//  first to portmapper, then to the server itself.  It keeps trying
//  until it succeeds, it is deactivated, or it is destroyed.  When it
//  succeeds, it calls its ConnectHandler.
//
//  An R.T.Connector is inactive when it's created.
//
//  The R.T.Connector uses an exponentially increasing timeout.  I.e.,
//  if its first attempt fails, it waits one second and tries again.
//  If the second attempt fails, it waits two seconds.  Then it waits
//  four seconds.  Then eight.  Et cetera, up to a maximum of 1024
//  seconds (~17 minutes).

class RPC_TCP_Connector {

public:

    typedef void (*ConnectHandler)(int fd, void *closure);

    RPC_TCP_Connector(unsigned long program,
                      unsigned long version,
                      unsigned long host,
		      ConnectHandler,
                      void *closure);
    ~RPC_TCP_Connector();

    bool active()			{ return state != IDLE; }
    void activate();
    void deactivate();

private:

    enum { PMAP_TIMEOUT = 60 };		// seconds
    enum { INITIAL_RETRY_INTERVAL = 1, MAX_RETRY_INTERVAL = 1024 }; // seconds
    enum State { IDLE, PMAPPING, CONNECTING, PAUSING };

    //  Instance Variables

    State state;
    int sockfd;
    sockaddr_in address;
    unsigned long program;
    unsigned long version;
    int retry_interval;
    const ConnectHandler connect_handler;
    void *closure;

    //  Private Instance Methods

    void try_to_connect();
    void try_again();

    //  Class Methods

    static void retry_task(void *closure);
    static void write_handler(int fd, void *closure);

};

#endif /* !RPCTCPConnector_included */

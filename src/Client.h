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

#ifndef Client_included
#define Client_included

#include <sys/types.h>
#include <netinet/in.h>  // for in_addr

#include "Activity.h"
#include "Boolean.h"
#include "Request.h"

class Cred;
class Event;
class Interest;
class Scanner;

//  Client is an abstract base type for clients of fam.  A Client has
//  a name which is used for debugging, and it receives events from
//  lower-level code.
//
//  These are the classes derived from Client.
//
//	InternalClient	some part of fam that needs to watch a file
//	MxClient	multiplexed client, an abstract type
//	TCP_Client	client that connects via TCP/IP protocol
//	RemoteClient	TCP_Client on a different machine

class Client {

public:    

    static in_addr LOCALHOST();

    Client(const char *name, in_addr host);
    virtual ~Client();

    virtual bool ready_for_events() = 0;
    virtual void post_event(const Event&, Request, const char *name) = 0;
    virtual void enqueue_for_scan(Interest *) = 0;
    virtual void dequeue_from_scan(Interest *) = 0;
    virtual void enqueue_scanner(Scanner *) = 0;
    virtual void suggest_insecure_compat(const char *) { return; }
    in_addr host() const        { return myhost; }
    const char *name();

protected:

    void name(const char *);

private:

    char *myname;
    in_addr myhost;

    Client(const Client&);		// Do not copy
    Client & operator = (const Client&);		//   or assign

};

#endif /* !Client_included */

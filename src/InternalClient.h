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

#ifndef InternalClient_included
#define InternalClient_included

#include "Client.h"

class ClientInterest;

//  An InternalClient is a hook for fam to use itself to monitor files.
//  It's used to monitor /etc/exports and /etc/mtab, and possibly others.
//
//  An InternalClient monitors a single file and calls a callback function
//  with any events received.  An InternalClient cannot monitor a directory.

class InternalClient : public Client {

public:

    typedef void (*EventHandler)(const Event&, void *closure);

    InternalClient(const char *filename, EventHandler, void *closure);
    ~InternalClient();

    bool ready_for_events();
    void post_event(const Event&, Request, const char *name);
    void enqueue_for_scan(Interest *);
    void dequeue_from_scan(Interest *);
    void enqueue_scanner(Scanner *);

private:

    EventHandler handler;
    void *closure;
    ClientInterest *interest;

};

#endif /* !InternalClient_included */

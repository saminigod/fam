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

#include "InternalClient.h"

#include <assert.h>

#include "Directory.h"
#include "Event.h"
#include "File.h"
#include "Log.h"

InternalClient::InternalClient(const char *filename,
			       EventHandler h, void *closr)
    : Client("myself", LOCALHOST()), handler(h), closure(closr)
{
    assert(filename);
    assert(h);
    assert(filename[0] == '/');
    Log::debug("%s watching %s", name(), filename);
    interest = new File(filename, this, Request(0), Cred::SuperUser);
}

InternalClient::~InternalClient()
{
    delete interest;
}

bool
InternalClient::ready_for_events()
{
    return true;			// always ready for events
}

void
InternalClient::enqueue_for_scan(Interest *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::dequeue_from_scan(Interest *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::enqueue_scanner(Scanner *)
{
    int function_is_never_called = 0; assert(function_is_never_called);
}

void
InternalClient::post_event(const Event& event, Request, const char *)
{
//  Log::debug("sent %s event: \"%s\" %s", Client::name(), name, event.name());
    (*handler)(event, closure);
}

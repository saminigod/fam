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

#include <stdlib.h>
#include "MxClient.h"

#include <assert.h>

#include "DirEntry.h"
#include "Directory.h"
#include "Event.h"
#include "File.h"
#include "Log.h"

MxClient::MxClient(in_addr host)
    : Client(NULL, host)
{
}

MxClient::~MxClient()
{
    while (requests.size()) 
    {
        Request r = requests.first();
	delete interest(r);	// Destroy all interests.
        requests.remove(r);
    }
}

//  MxClient::interest() maps a request number to a ClientInterest ptr.

ClientInterest *
MxClient::interest(Request r)
{
    ClientInterest *ip = requests.find(r);
    if (!ip)
	Log::error("%s invalid request number %d", name(), r);
    return ip;
}

bool
MxClient::check_new(Request request, const char *path)
{
    if (path[0] != '/')
    {   Log::info("relative path \"%s\" rejected", path);
	post_event(Event::Acknowledge, request, path);
	return false;
    }

    if (requests.find(request))
    {   Log::error("%s nonunique request number %d rejected", name(), request);
	return false;
    }

    return true;
}

void
MxClient::monitor_file(Request request, const char *path, const Cred& cred)
{
    if (check_new(request, path))
    {
	ClientInterest *ip = new File(path, this, request, cred);
	requests.insert(request, ip);
    }
}

void
MxClient::monitor_dir(Request request, const char *path, const Cred& cred)
{
    if (check_new(request, path))
    {
	ClientInterest *ip = new Directory(path, this, request, cred);
	requests.insert(request, ip);
    }
}

void
MxClient::suspend(Request r)
{
    ClientInterest *ip = interest(r);
    if (ip)
	ip->suspend();
}

void
MxClient::resume(Request r)
{
    ClientInterest *ip = interest(r);
    if (ip)
	ip->resume();
}

void
MxClient::cancel(Request r)
{
    ClientInterest *ip = interest(r);
    if (ip) {
        requests.remove(r);
        ip->cancel();
        delete ip;
    }
}

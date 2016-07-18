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

#include "ClientInterest.h"

#include <assert.h>
#include <errno.h>

#include "Client.h"
#include "Event.h"
#include "FileSystem.h"
#include "FileSystemTable.h"

ClientInterest::ClientInterest(const char *name,
			       Client *c, Request r, const Cred& cr, Type type)
    : Interest(name, myfilesystem = FileSystemTable::find(name, cr),
               c->host(), VERIFY_EXPORTED),
      myclient(c), request(r), mycred(cr), fs_request(0)
{
    ci_bits() = ACTIVE_STATE;
    if (exported_to_host())
    {
        //  This is so horribly lame.  If this interest doesn't exist,
        //  and someone somewhere set errno to EACCES, let's assume the
        //  interest exists but this client doesn't have permission to
        //  monitor it.  (The actual imon_express and lstat are done in
        //  in the Interest constructor.)  This use of errno probably
        //  makes it easy to become confused and suggest_insecure_compat
        //  even when we shouldn't.
        if ((!exists()) && (errno == EACCES) && (c != NULL))
        {
            c->suggest_insecure_compat(name);
        }
        post_event(exists() ? Event::Exists : Event::Deleted);
        fs_request = myfilesystem->monitor(this, type);
    }
    else
    {
        post_event(Event::Deleted);
    }
}

ClientInterest::~ClientInterest()
{
    myfilesystem->cancel(this, fs_request);
}

void
ClientInterest::filesystem(FileSystem *fs)
{
    myfilesystem->cancel(this, fs_request);
    myfilesystem = fs;
    verify_exported_to_host();
    if (exported_to_host())
    {
        fs_request = fs->monitor(this, type());
        scan();
    }
    else if (fs_request != 0)
    {
        //  This Interest used to be on a filesystem which was mounted by
        //  the client, but now is not?  Send a Deleted event.
        post_event(Event::Deleted);
        fs_request = 0;
    }
}

void
ClientInterest::findfilesystem()
{
    FileSystem *new_fs = FileSystemTable::find(name(), mycred);
    if (new_fs != myfilesystem)
    {
        filesystem(new_fs);
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Suspend/resume.

bool
ClientInterest::active() const
{
    return ci_bits() & ACTIVE_STATE;
}

void
ClientInterest::suspend()
{
    if (active())
    {   ci_bits() &= ~ACTIVE_STATE;
	myfilesystem->hl_suspend(fs_request);
    }
}

void
ClientInterest::resume()
{
    bool was_active = active();
    ci_bits() |= ACTIVE_STATE;	// Set state before generating events.
    do_scan();
    if (!was_active)
	myfilesystem->hl_resume(fs_request);
}

void
ClientInterest::post_event(const Event& event, const char *eventpath)
{
    assert(active());
    if (!eventpath)
	eventpath = name();
    myclient->post_event(event, request, eventpath);
}

Interest *
ClientInterest::find_name(const char *)
{
    //  Ignore name.

    return this;
}

bool
ClientInterest::scan(Interest *ip)
{
    bool changed = false;
    if (!ip)
	ip = this;
    ip->mark_for_scan();
    if (myclient->ready_for_events())
	changed = ip->do_scan();
    else
	myclient->enqueue_for_scan(ip);
    return changed;
}

void
ClientInterest::unscan(Interest *ip)
{
    if (!ip)
	ip = this;
    if (ip->needs_scan())
	myclient->dequeue_from_scan(ip);
}

bool
ClientInterest::do_scan()
{
    bool changed = false;
    if (needs_scan() && active()) {
        become_user();
        changed = Interest::do_scan();
    }
    return changed;
}

void
ClientInterest::notify_created(Interest *ip)
{
    if (!ip)
	ip = this;
    myfilesystem->ll_notify_created(ip);
}

void
ClientInterest::notify_deleted(Interest *ip)
{
    if (!ip)
	ip = this;
    myfilesystem->ll_notify_deleted(ip);
}

void
ClientInterest::cancel() 
{
    ci_bits() |= ACTIVE_STATE;
    post_event(Event::Acknowledge);
    unscan();
}

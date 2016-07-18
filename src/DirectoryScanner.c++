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

#include "DirectoryScanner.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#include "Client.h"
#include "Directory.h"
#include "DirEntry.h"
#include "Log.h"

//////////////////////////////////////////////////////////////////////////////

DirectoryScanner::DirectoryScanner(Directory& d,
				   const Event& e, bool b,
				   DoneHandler dh, void *vp)
    : directory(d), done_handler(dh), closure(vp), new_event(e),
      scan_entries(b), dir(NULL), openErrno(0),
      epp(&d.entries), discard(NULL)
      
{
    dir = opendir(d.name());
    if (dir == NULL) {
	openErrno = errno;
    }
}

DirectoryScanner::~DirectoryScanner()
{
    if (dir)
	closedir(dir);
}

//////////////////////////////////////////////////////////////////////////////

// return address of ptr to entry matching name

DirEntry **
DirectoryScanner::match_name(DirEntry **epp, const char *name)
{
    for (DirEntry *ep; ((ep = *epp) != NULL); epp = &ep->next)
	if (!strcmp(ep->name(), name))
	    return epp;
    return NULL;
}

bool
DirectoryScanner::done()
{
#if HAVE_SGI_NOHANG    
    if (openErrno == ETIMEDOUT) {
        // We got an nfs time out.  We'll want to try again later.
	directory.unhang();
	Log::debug("openErrno == ETIMEDOUT");
	(*done_handler)(closure);
	return true;
    }
#endif
    bool ready = directory.client()->ready_for_events();

    directory.become_user();
    if (!directory.chdir()) {
        // Didn't have permission to read the directory.  Send Delete events
        // for its contents.
        while (*epp && ready)
        {   DirEntry *ep = *epp;
	    *epp = ep->next;
	    ep->post_event(Event::Deleted);
	    ready = directory.client()->ready_for_events();
	    delete ep;
        }
        if (*epp || !ready)
	    return false;

        (*done_handler)(closure);
        return true;
    }
    
    while (dir && ready)
    {
	struct direct *dp = readdir(dir);
	if (dp == NULL)
	{   closedir(dir);
	    dir = NULL;
	    break;
	}

	//  Ignore "." and "..".

	if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
	    continue;

	DirEntry *ep = *epp, **epp2;
	if (ep && !strcmp(dp->d_name, ep->name()))
	{
	    //  Next entry in list matches. Do not change list.

	    // Log::debug("checkdir match %s", dp->d_name);
	}
	else if ((epp2 = match_name(&discard, dp->d_name)) != NULL)
	{
	    //  Found in discard.  Insert discarded entry before ep.

	    // Log::debug("checkdir fdisc %s", dp->d_name);
	    ep = *epp2;
	    *epp2 = ep->next;
	    ep->next = *epp;
	    *epp = ep;
	}
	else if (ep && (epp2 = match_name(&ep->next, dp->d_name)))
	{
	    //  Found further in list.  Prepend internode segment
	    //  to discard.

	    // Log::debug("checkdir furth %s", dp->d_name);
	    ep = *epp2;
	    *epp2 = discard;
	    discard = *epp;
	    *epp = ep;
	}
	else
	{
	    // New entry. Insert.

	    ep = new DirEntry(dp->d_name, &directory, *epp);
	    *epp = ep;
	    ep->post_event(new_event);
	    ready = directory.client()->ready_for_events();
	    epp = &ep->next;
	    continue;		// Do not scan newly created entry.
	}
	if (scan_entries)
	{   ep->scan_no_chdir();
	    ready = directory.client()->ready_for_events();
	}
	epp = &ep->next;
    }

    directory.chdir_root();  // chdir back to "/"
    
    while (*epp && ready)
    {   DirEntry *ep = *epp;
	*epp = ep->next;
	ep->post_event(Event::Deleted);
	ready = directory.client()->ready_for_events();
	delete ep;
    }

    while (discard && ready)
    {   DirEntry *ep = discard;
	discard = discard->next;
	ep->post_event(Event::Deleted);
	ready = directory.client()->ready_for_events();
	delete ep;
    }
	
    if (dir || *epp || discard || !ready)
	return false;

    (*done_handler)(closure);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//  Memory management.  Maintain a cache of one DirectoryScanner to reduce
//  heap stirring.

DirectoryScanner *DirectoryScanner::cache;

void *
DirectoryScanner::operator new (size_t size)
{
    assert(size == sizeof (DirectoryScanner));
    DirectoryScanner *p = cache ? cache : (DirectoryScanner *) new char[size];
    if (cache)
	cache = NULL;
    return p;
}

void
DirectoryScanner::operator delete (void *p)
{
    assert(p != NULL);
    if (cache)
	delete [] cache;
    cache = (DirectoryScanner *) p;
}

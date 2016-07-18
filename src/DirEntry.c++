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

#include "DirEntry.h"

#include <assert.h>
#include <stdio.h>
#include <sys/param.h>

#include "Directory.h"

// A DirEntry may be polled iff its parent is not polled.

DirEntry::DirEntry(const char *name, Directory *p, DirEntry *nx)
    : Interest(name, p->filesystem(), p->host(), NO_VERIFY_EXPORTED),
      parent(p), next(nx), need_to_chdir(true)
{ }

DirEntry::~DirEntry()
{
    unscan();
}

bool
DirEntry::active() const
{
    return parent->active();
}

void
DirEntry::post_event(const Event& event, const char *eventpath)
{
    assert(!eventpath);
    parent->post_event(event, name());
}

bool
DirEntry::scan(Interest *ip)
{
    assert(!ip);
    return parent->scan(this);
}

void
DirEntry::scan_no_chdir()
{
    need_to_chdir = false;
    parent->scan(this);
    need_to_chdir = true;

}

void
DirEntry::unscan(Interest *ip)
{
    assert(!ip);
    parent->unscan(this);
}

bool
DirEntry::do_scan()
{
    bool changed = false;
    if (needs_scan() && active()) {
        if (need_to_chdir) {
            parent->become_user();
            if (!parent->chdir()) {
                return false;
            }
        }
        changed = Interest::do_scan();
        if (need_to_chdir) {
            parent->chdir_root();
        }
    }
    return changed;
}

void
DirEntry::notify_created(Interest *ip)
{
    assert(ip == this);
    parent->notify_created(ip);
}

void 
DirEntry::notify_deleted(Interest *ip)
{
    assert(ip == this);
    parent->notify_deleted(ip);
}

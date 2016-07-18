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

#include "LocalFileSystem.h"

#include <string.h>

#include "Log.h"
#include "Pollster.h"

LocalFileSystem::LocalFileSystem(const mntent& mnt)
    : FileSystem(mnt)
{ }

bool
LocalFileSystem::dir_entries_scanned() const
{
    return true;
}

int
LocalFileSystem::get_attr_cache_timeout() const
{
    int filesystem_is_NFS = 0; assert(filesystem_is_NFS);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//  High level interface

Request
LocalFileSystem::hl_monitor(ClientInterest *, ClientInterest::Type)
{
    return 0;
}

void
LocalFileSystem::hl_cancel(Request)
{ }

void
LocalFileSystem::hl_suspend(Request)
{ }

void
LocalFileSystem::hl_resume(Request)
{ }

void
LocalFileSystem::hl_map_path(char *remote_path, const char *local_path,
			     const Cred&)
{
    (void) strcpy(remote_path, local_path);
}

//////////////////////////////////////////////////////////////////////////////
//  Low level interface

void
LocalFileSystem::ll_monitor(Interest *ip, bool imonitored)
{
    if (!imonitored)
    {
	Log::debug("will poll %s", ip->name());
	Pollster::watch(ip);
    }
}

void
LocalFileSystem::ll_notify_created(Interest *ip)
{
    Pollster::forget(ip);
}


void
LocalFileSystem::ll_notify_deleted(Interest *ip)
{
    Pollster::watch(ip);
}

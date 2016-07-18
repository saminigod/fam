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

#include "FileSystem.h"

#include <mntent.h>
#include <string.h>

#include "Event.h"

FileSystem::FileSystem(const mntent& mnt)
    : mydir   (strcpy(new char[strlen(mnt.mnt_dir   ) + 1], mnt.mnt_dir   )),
      myfsname(strcpy(new char[strlen(mnt.mnt_fsname) + 1], mnt.mnt_fsname))
{ }

FileSystem::~FileSystem()
{
    assert(!myinterests.size());
    delete [] mydir;
    delete [] myfsname;
}

bool
FileSystem::matches(const mntent& mnt) const
{
    return !strcmp(mydir, mnt.mnt_dir) && !strcmp(myfsname, mnt.mnt_fsname);
}

void
FileSystem::relocate_interests()
{
    for (ClientInterest *cip = myinterests.first();
	 cip;
	 cip = myinterests.next(cip))
    {
	cip->findfilesystem();
    }
}

Request
FileSystem::monitor(ClientInterest *cip, ClientInterest::Type type)
{
    myinterests.insert(cip);
    return hl_monitor(cip, type);
}

void
FileSystem::cancel(ClientInterest *cip, Request request)
{
    hl_cancel(request);
    myinterests.remove(cip);
}

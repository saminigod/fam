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

#include "config.h"
#include "NFSFileSystem.h"

#include <assert.h>
#include <mntent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Log.h"
#include "ServerHost.h"

#if HAVE_SYS_FS_NFS_CLNT_H
#include <rpc/types.h>
#include <sys/fs/nfs.h>
#include <sys/fs/nfs_clnt.h>
#else
#define ACREGMAX 60
#define ACREGMIN 3
#endif

NFSFileSystem::NFSFileSystem(const mntent& mnt)
    : FileSystem(mnt)
{
    //  Extract the host name from the fs name.

    const char *fsname = mnt.mnt_fsname;
    const char *colon = strchr(fsname, ':');
    if(colon == NULL)
    {
        //  shouldn't happen, as this is checked in
        //  FileSystemTable::create_fs_by_name().
        assert(colon);
        colon = fsname;
    }
    char hostname[NAME_MAX + 1];
    int hostnamelen = colon - fsname;
    if(hostnamelen > NAME_MAX)
    {
        assert(hostnamelen <= NAME_MAX);
        hostnamelen = NAME_MAX;
    }
    strncpy(hostname, fsname, hostnamelen);
    hostname[hostnamelen] = '\0';

    //  Look up the host by name.

    host = ServerHostRef(hostname);

    //  Store the remote directory name.  As a special case, "/"
    //  is translated to "" so it will prepend neatly to an absolute
    //  path.

    const char *dir_part = colon + 1;
    if (!strcmp(dir_part, "/"))
	dir_part++;
    remote_dir_len = strlen(dir_part);
    remote_dir = strcpy(new char[remote_dir_len + 1], dir_part);
    local_dir_len = strlen(dir());

    // Figure out what the attribute cache time is.  Look for the
    // acregmin, acregmax, actimeo, and noac options in the mount
    // options.  Otherwise, use defaults.

    const char * opt = mnt.mnt_opts;

    bool f_noac = false;
    bool f_actimeo = false;
    bool f_acregmin = false;
    bool f_acregmax = false;

    int actimeo;
    int acregmin;
    int acregmax;

    attr_cache_timeout = ACREGMAX;

    char * p;
        
    if (strstr(opt, "noac")) {
        f_noac = true;
    }
    if ((p = strstr(opt, "actimeo"))) 
    {
        if (sscanf(p, "actimeo=%i", &actimeo) == 1) {
            f_actimeo = true;
        }
    }
    
    if ((p = strstr(opt, "acregmin"))) {
        if (sscanf(p, "acregmin=%i", &acregmin) == 1) {
            f_acregmin = true;
        }
    }
    
    if ((p = strstr(opt, "acregmax"))) {
        if (sscanf(p, "acregmax=%i", &acregmax) == 1) {
            f_acregmax = true;
        }
    }
    
    if (f_noac) {
        if (!f_actimeo && !f_acregmin && !f_acregmax) {
            attr_cache_timeout = 0;
        } else {
            Log::error("Both noac and (actimeo, acregmin, or acregmax) "
                       "were set");
        }
    } else if (f_actimeo) {
        if (!f_acregmin && !f_acregmax) {
            attr_cache_timeout = actimeo;
        } else {
            Log::error("Both actimeo and (acregmin or acregmax) were set");
        }
    } else if (f_acregmax) {
        if (f_acregmin) {
            if (acregmin <= acregmax) {
                attr_cache_timeout = acregmax;
            } else {
                Log::error("Both acregmax and acregmin were set, but "
                           "acregmin was greater than acregmax.");
            }
        } else {
            if (ACREGMIN <= acregmax) {
                attr_cache_timeout = acregmax;
            } else {
                Log::error("acregmax was less than the default for acregmin");
            }
        }
    } else if (f_acregmin) {
        if (acregmin < ACREGMAX) {
            attr_cache_timeout = ACREGMAX;
        } else {
            attr_cache_timeout = acregmin;
        }
    } else {
        attr_cache_timeout = ACREGMAX;
    }
    Log::debug("attr_cache_timout set to %i", attr_cache_timeout);
}

NFSFileSystem::~NFSFileSystem()
{
    ClientInterest *cip;
    while ((cip = interests().first()) != NULL)
	cip->findfilesystem();
    delete [] remote_dir;
}

bool
NFSFileSystem::dir_entries_scanned() const
{
    return !host->is_connected();
}


int
NFSFileSystem::get_attr_cache_timeout() const
{
    return attr_cache_timeout;
}

//////////////////////////////////////////////////////////////////////////////
//  High level interface

Request
NFSFileSystem::hl_monitor(ClientInterest *ci, ClientInterest::Type type)
{
    const char *path = ci->name();
    assert(path[0] == '/');
    char remote_path[PATH_MAX];
    hl_map_path(remote_path, ci->name(), ci->cred());
    return host->send_monitor(ci, type, remote_path);
}

void
NFSFileSystem::hl_cancel(Request request)
{
    host->send_cancel(request);
}

void
NFSFileSystem::hl_suspend(Request request)
{
    host->send_suspend(request);
}

void
NFSFileSystem::hl_resume(Request request)
{
    host->send_resume(request);
}

void
NFSFileSystem::hl_map_path(char *remote_path, const char *path, const Cred& cr)
{
    char local_path[PATH_MAX];
    cr.become_user();
    if (realpath(path, local_path))
    {   assert(!strncmp(local_path, dir(), local_dir_len));
	(void) strcpy(remote_path, remote_dir);
	(void) strcpy(remote_path + remote_dir_len,
		       local_path + local_dir_len);
    }
    else
    {
	//  realpath failed -- remove components one at a time
	//  until it starts succeeding, then append the
	//  missing components to the path.  Use remote_path
	//  for scratch space.

	(void) strcpy(remote_path, path);
	char *p;
	do {
	    p = strrchr(remote_path, '/');
	    assert(p);
	    *p = '\0';
	} while (!realpath(remote_path, local_path));
	(void) strcpy(remote_path, remote_dir);
	(void) strcpy(remote_path + remote_dir_len,
		      local_path + local_dir_len);
	(void) strcat(remote_path, path + (p - remote_path));
    }

    // If we're famming a remote machine's root directory, then
    // remote_path will be empty at this point.  Make it "/" instead.
    if (!*remote_path) {
        strcpy(remote_path, "/");
    }
        
    //	Log::debug("NFSFileSystem::hl_map_path() mapped \"%s\" -> \"%s\"",
    //		   path, remote_path);
}

//////////////////////////////////////////////////////////////////////////////
//  Low level interface: no implementation.

void
NFSFileSystem::ll_monitor(Interest *, bool)
{ }

void
NFSFileSystem::ll_notify_created(Interest *)
{ }

void
NFSFileSystem::ll_notify_deleted(Interest *)
{ }

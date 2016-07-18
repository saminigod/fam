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

#ifndef NFSFileSystem_included
#define NFSFileSystem_included

#include "FileSystem.h"
#include "ServerHostRef.h"

//  NFSFileSystem represents an NFS file system.
//
//  NFSFileSystem implements the high level FileSystem interface.  It
//  has a null implementation of the low level interface.  (See
//  FileSystem.h for the high and low level interfaces.)
//
//  Perhaps the most significant thing NFSFileSystem does is mapping
//  local paths to remote paths (hl_map_path()).

class NFSFileSystem : public FileSystem {

public:

    NFSFileSystem(const mntent&);
    ~NFSFileSystem();

    virtual bool dir_entries_scanned() const;
    virtual int get_attr_cache_timeout() const;

    // High level monitoring interface

    virtual Request hl_monitor(ClientInterest *, ClientInterest::Type);
    virtual void    hl_cancel(Request);
    virtual void    hl_suspend(Request);
    virtual void    hl_resume(Request);
    virtual void    hl_map_path(char *remote_path, const char *local_path,
				const Cred&);

    // Low level monitoring interface

    virtual void ll_monitor(Interest *, bool);
    virtual void ll_notify_created(Interest *);
    virtual void ll_notify_deleted(Interest *);

private:

    ServerHostRef host;
    char *remote_dir;
    unsigned remote_dir_len;
    unsigned local_dir_len;
    int attr_cache_timeout;

};

#endif /* !NFSFileSystem_included */

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

#ifndef LocalFileSystem_included
#define LocalFileSystem_included

#include "FileSystem.h"

//  LocalFileSystem represents a local (non-NFS) file system.
//
//  LocalFileSystem has a null implementation of the high level
//  interface.  Its low level interface puts nonexistent files 
//  on the list to be polled.
//
//  Note that a LocalFileSystem may represent a weird file system
//  like a DOS floppy or and ISO-9660 CD-ROM.  Don't assume efs/xfs/ext2...

class LocalFileSystem : public FileSystem {

public:

    LocalFileSystem(const mntent&);

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

    virtual void ll_monitor(Interest *, bool imonitored);
    virtual void ll_notify_created(Interest *);
    virtual void ll_notify_deleted(Interest *);

};

#endif /* !LocalFileSystem_included */

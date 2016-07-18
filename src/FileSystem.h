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

#ifndef FileSystem_included
#define FileSystem_included

#include "ClientInterest.h"
#include "Request.h"
#include "Set.h"

struct mntent;
struct stat;

//  FileSystem is the abstract base class for a per-filesystem object.
//  A FileSystem has two monitoring interfaces: high level and low
//  level.  It also maintains a set of all ClientInterests on the
//  file system
//
//  It also has miscellaneous access functions:
//
//	dir() returns the mount point.
//
//	fsname() returns the device name.
//
//	matches() tests whether a given struct mntent describes the
//	current filesystem.
//
//	interests() returns a set of all ClientInterests on the
//	filesystem.
//
//  A FileSystem also can relocate its interests (after it's been
//  dismounted or another FileSystem has been mounted on it), by calling
//  relocate_interests().  This checks all Interests to see what FileSystem
//  they're on, and moves them appropriately.
//
//  The high level interface supports monitoring a file whether it
//  exists or not, and can monitor all entries in a directory.
//  The high level interface corresponds to what fam can rely
//  on from a remote fam on an NFS server.
//  
//  The high level interface is:
//
//	monitor()
//	cancel()
//	hl_suspend()
//	hl_resume()
//	hl_map_path()
//  
//  See the strong resemblance to FAM's client interface? (-:
//  hl_map_path() maps a local pathname to a remote pathname.
//  For a local FileSystem, this is trivial.  For a remote
//  file system, it's less trivial.
//
//  The low level interface monitors a single filesystem entity
//  and must be informed if the entity is a special file or does
//  not exist.  The low level interface corresponds to what fam
//  needs for local files.
//
//  The low level interface is:
//
//	ll_monitor()		start monitoring an interest.
//	ll_notify_created()	notify the FS that the interest was deleted.
//	ll_notify_deleted()	notify the FS that the interest was created.
//
//  The two subclasses of FileSystem are LocalFileSystem and
//  NFSFileSystem.  LocalFileSystem implements the low level
//  interface.  NFSFileSystem implements the high level interface.

class FileSystem {

public:

    typedef Set<ClientInterest *> Interests;

    FileSystem(const mntent&);
    virtual ~FileSystem();

    //  Miscellaneous routines

    bool matches(const mntent& m) const;
    const char *dir() const		{ return mydir; }
    const char *fsname() const		{ return myfsname; }
    const Interests& interests()	{ return myinterests; }
    virtual bool dir_entries_scanned() const = 0;
    void relocate_interests();
    virtual int get_attr_cache_timeout() const = 0;

    //  High level monitoring interface

    Request      monitor(ClientInterest *, ClientInterest::Type);
    void         cancel(ClientInterest *, Request);
    virtual void hl_suspend(Request) = 0;
    virtual void hl_resume(Request) = 0;
    virtual void hl_map_path(char *remote_path, const char *local_path,
			     const Cred& cr) = 0;

    //  Low level monitoring interface

    virtual void ll_monitor(Interest *, bool imonitored) = 0;
    virtual void ll_notify_created(Interest *) = 0;
    virtual void ll_notify_deleted(Interest *) = 0;

private:

    //  Instance Variables

    char *mydir;
    char *myfsname;
    Interests myinterests;

    virtual Request hl_monitor(ClientInterest *, ClientInterest::Type) = 0;
    virtual void    hl_cancel(Request) = 0;

};

#endif /* !FileSystem_included */

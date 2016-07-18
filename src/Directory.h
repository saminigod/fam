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

#ifndef Directory_included
#define Directory_included

#include "ClientInterest.h"

class DirEntry;
class DirectoryScanner;

//  A Directory represents a directory that we are monitoring.
//  It's derived from ClientInterest.  See the comments in
//  ClientInterest and Interest.
//
//  Each Directory has a linked list of DirEntries.  The DirEntries
//  are stored in the order they're returned by readdir(2).

class Directory : public ClientInterest {

public:

    Directory(const char *name, Client *, Request, const Cred&);
    ~Directory();

    virtual void resume();

    Type type() const;
    Interest *find_name(const char *);
    virtual bool do_scan();
    static bool chdir_root();

    bool chdir();

#if HAVE_SGI_NOHANG    
    void unhang();
#endif
    
private:

    enum { SCANNING = 1 << 0, RESCAN_SCHEDULED = 1 << 1 };

    //  Instance Variable

    DirEntry *entries;

    pid_t unhangPid;

    //  Class Variable

    static Directory *current_dir;

    //  Class Methods

    static void new_done_handler(void *);
    static void scan_done_handler(void *);
    static void scan_task(void *);

friend class DirEntry;
friend class DirectoryScanner;

};

#endif /* !Directory_included */

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

#ifndef DirEntry_included
#define DirEntry_included

#include "Interest.h"

class Directory;

//  A DirEntry is a subclass of Interest that represents an entry
//  in a Directory that a Client has expressed an interest in.
//
//  A DirEntry overrides a lot of Interest's methods and forwards
//  them to its parent directory.
//
//  DirEntries are stored on a singly-linked list.  The head of the
//  list is in the parent.  Each DirEntry also has a parent pointer.
//  The entries in the list are in the order they're returned by
//  readdir().

class DirEntry : public Interest {

public:

    virtual bool active() const;
    virtual bool scan(Interest * = 0);
    virtual void scan_no_chdir();
    virtual void unscan(Interest * = 0);
    virtual bool do_scan();

protected:

    void post_event(const Event&, const char * = 0);

private:

    //  Instance Variables

    Directory *const parent;
    DirEntry *next;

    bool need_to_chdir;
    
    //  Private Instance Methods

    DirEntry(const char *name, Directory *parent, DirEntry *next);
				// Only a Directory may create a DirEntry.
    virtual ~DirEntry();

    virtual void notify_created(Interest *);
    virtual void notify_deleted(Interest *);

friend class Directory;
friend class DirectoryScanner;

};

#endif /* !DirEntry_included */

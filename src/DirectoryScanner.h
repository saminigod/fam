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

#ifndef DirectoryScanner_included
#define DirectoryScanner_included

#include "Scanner.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/dir.h>

#include "Event.h"

class Client;
class Directory;
class DirEntry;

//  DirectoryScanner scans a directory for created or deleted files.
//  It is an object because a scan can be interrupted when the output
//  stream becomes blocked.
//
//  The interface is quite weird.  Each time the done() method is called,
//  it does as much work as it can, until output is blocked.  Then
//  it returns true or false depending on whether it got done.
//
//  This whole flow control thing needs a good redesign.
//
//  Since a large number of DirectoryScanners is created, we have our
//  own new and delete operators.  They cache the most recently freed
//  DirectoryScanner for re-use.

class DirectoryScanner : public Scanner {

public:

    typedef void (*DoneHandler)(void *closure);

    DirectoryScanner(Directory&, const Event&, bool, DoneHandler, void *);
    virtual ~DirectoryScanner();

    virtual bool done();

    void *operator new (size_t);
    void operator delete (void *);

private:

    //  Instance Variables

    Directory& directory;
    const DoneHandler done_handler;
    void *const closure;
    const Event& new_event;
    const bool scan_entries;
    DIR *dir;
    int openErrno;
    DirEntry **epp;
    DirEntry *discard;

    //  Class Variable

    static DirectoryScanner *cache;

    //  Private Instance Methods

    DirEntry **match_name(DirEntry **epp, const char *name);

};

#endif /* !DirectoryScanner_included */

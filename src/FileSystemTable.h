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

#ifndef FileSystemTable_included
#define FileSystemTable_included

#include "config.h"
#include "SmallTable.h"
#include "StringTable.h"

class Cred;
class Event;
class FileSystem;
class InternalClient;

//  FileSystemTable provides a static function, find(), which looks up
//  a path and returns a pointer to the FileSystem where that path
//  resides.

class FileSystemTable {

public:

#ifdef HAPPY_PURIFY
    FileSystemTable();
    ~FileSystemTable();
#endif

    static FileSystem *find(const char *path, const Cred& cr); 

private:

    typedef SmallTable<unsigned long, FileSystem *> IDTable;
    typedef StringTable<FileSystem *> NameTable;

    //  Class Variables

    static const char mtab_name[];
    static unsigned int count;
    static IDTable    fs_by_id;
    static NameTable *fs_by_name;
    static InternalClient *mtab_watcher;
    static FileSystem *root;

    //  Class Methods

    static void create_fs_by_name();
    static void destroy_fses(NameTable *);
    static FileSystem *longest_prefix(const char *path);
    static void mtab_event_handler(const Event&, void *);

};

#ifdef HAPPY_PURIFY
static FileSystemTable FileSystemTable_instance;
#endif

#endif /* !FileSystemTable_included */

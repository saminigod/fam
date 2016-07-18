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

#ifndef ServerHostRef_included
#define ServerHostRef_included

#include "ServerHost.h"
#include "StringTable.h"

//  ServerHostRef is a reference-counted pointer to a ServerHost.
//
//  A ServerHostRef is created by specifying the name of a
//  host.  The pointed-to ServerHost will be shared with other
//  Refs to the same host, even under different names.

class ServerHostRef {

public:

    ServerHostRef()			: p(0) { count++; }
    ServerHostRef(const char *name)	: p(find(name))
					{ p->refcount++; count++; }
    ServerHostRef(const ServerHostRef& that)
					: p(that.p)
					{ if (p) p->refcount++; count++; }
    ServerHostRef& operator = (const ServerHostRef&);
    ~ServerHostRef();

    ServerHost *operator -> ()		{ assert(p); return p; }
    const ServerHost *operator -> () const { assert(p); return p; }

private:

    // Instance Variable

    ServerHost *p;

    //  Class Variables

    static unsigned count;		// number of handles extant
    static StringTable<ServerHost *> *hosts_by_name;

    //  Private Instance Methods

    ServerHostRef(ServerHost& that)	: p(&that) { p->refcount++; count++; }

    //  Private Class Methods

    static ServerHost *find(const char *name);

};

#endif /* !ServerHost_included */

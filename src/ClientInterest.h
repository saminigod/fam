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

#ifndef ClientInterest_included
#define ClientInterest_included

#include "Boolean.h"
#include "Cred.h"
#include "Interest.h"
#include "Request.h"

class Client;

//  ClientInterest -- an abstract base class for a filesystem entity
//  in which a client has expressed an interest.  The two kinds of
//  ClientInterest are File and Directory.
//
//  The ClientInterest is intimately tied to the Interest, Directory
//  and DirEntry.  And the whole hierarchy is very messy.

class ClientInterest : public Interest {

public:

    enum Type { FILE = 'W', DIRECTORY = 'M' };

    virtual ~ClientInterest();

    virtual void suspend();
    virtual void resume();
    virtual bool active() const;

    void findfilesystem();
    bool scan(Interest * = NULL);
    void unscan(Interest * = NULL);
    virtual Interest *find_name(const char *);
    virtual bool do_scan();

    void become_user() const		{ mycred.become_user(); }
    const Cred& cred() const		{ return mycred; }
    FileSystem *filesystem() const	{ return myfilesystem; }
    Client *client() const		{ return myclient; }
    virtual Type type() const = 0;

    virtual void notify_created(Interest *);
    virtual void notify_deleted(Interest *);

    virtual void cancel();
    
protected:

    ClientInterest(const char *name, Client *, Request, const Cred&, Type);
    void post_event(const Event&, const char * = NULL);

private:

    enum { ACTIVE_STATE = 1 << 0 };

    //  Instance Variables

    Client *myclient;
    Request request;
    const Cred mycred;
    FileSystem *myfilesystem;
    Request fs_request;

    void filesystem(FileSystem *);

};

#endif /* !ClientInterest_included */

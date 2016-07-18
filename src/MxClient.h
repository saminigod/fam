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

#ifndef MxClient_included
#define MxClient_included

#include "Client.h"
#include "RequestMap.h"

class ClientInterest;

//  MxClient is a multiplexed client (one with more than one request
//  outstanding).  It an an abstract base type whose most famous
//  derived class is TCP_Client.
//  
//  An MxClient maps Requests to ClientInterests.  It also implements
//  (part of) the suspend/resume protocol.
//
//  In a glorious spasm of overengineering, I implemented the table of
//  requests as an in-core B-Tree.  Why?  Because a B-Tree scales
//  really well and because it has good locality of reference.

class MxClient : public Client {

protected:

    MxClient(in_addr host);
    ~MxClient();

    void monitor_file(Request, const char *path, const Cred&);
    void monitor_dir(Request, const char *path, const Cred&);
    void cancel(Request);
    void suspend(Request);
    void resume(Request);

private:

    RequestMap requests;

    ClientInterest *interest(Request);
    bool check_new(Request, const char *path);

};

#endif /* !MxClient_included */

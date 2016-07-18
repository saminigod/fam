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

#ifndef Listener_included
#define Listener_included

#include <sys/types.h>

#include "Boolean.h"

//  Listener is an object that listens for connections on a socket.
//  It registers itself with portmapper.
//
//  If a Listener was started_by_inetd, then it listens on the
//  already-open file descriptor 0.  Otherwise, it opens a new socket.
//
//  A Listener creates TCP_Client and RemoteClient objects as
//  connections come in.
//
//  FAMPROG and FAMVERS, the SUN RPC program number and version, are
//  defined here, because I didn't think of a better place to define them.

class TCP_Client;

class Listener {

public:

    enum { RENDEZVOUS_FD = 0 };		// descriptor 0 opened by inetd
    enum { FAMPROG = 391002, FAMVERS = 2 };


    Listener(bool started_by_inetd,
             bool local_only,
	     unsigned long program = FAMPROG, unsigned long version = FAMVERS);
    ~Listener();

    static void create_local_client(TCP_Client &inet_client, uid_t uid);

private:

    //  Instance Variables

    unsigned long program;
    unsigned long version;
    int rendezvous_fd;
    bool started_by_inetd;
    int _ugly_sock;
    bool local_only;

    //  Private Instance Methods

    void dirty_ugly_hack();
    void set_rendezvous_fd(int);

    //  Class Methods

    static void accept_client(int fd, void *closure);
    static void accept_ugly_hack(int fd, void *closure);
    static void read_ugly_hack(int fd, void *closure);
    static void accept_localclient(int fd, void *closure);
};

#endif /* !Listener_included */

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

#ifndef TCP_Client_included
#define TCP_Client_included

#include "ClientConnection.h"
#include "MxClient.h"
#include "Set.h"
#include "Cred.h"

struct sockaddr_un;

//  A TCP_Client is a client that connects to fam using the TCP/IP
//  protocol.  This class implements the fam protocol, and it also
//  handles I/O buffering.
//
//  The order of TCP_Client's fields has been optimized.  The
//  connection is next to last because it contains big buffers, and
//  the ends of the buffers will rarely be accessed.  The Activity
//  is last because it is never accessed.

class TCP_Client : public MxClient {

public:

    TCP_Client(in_addr host, int fd, Cred &cred);
    ~TCP_Client();

    bool ready_for_events();
    void post_event(const Event&, Request, const char *name);
    void enqueue_for_scan(Interest *);
    void dequeue_from_scan(Interest *);
    virtual void enqueue_scanner(Scanner *);
    virtual void suggest_insecure_compat(const char *path);

    void send_sockaddr_un(sockaddr_un &sun) { conn.send_sockaddr_un(sun); }

protected:
    Cred cred;  //  if !is_valid, we believe the uid/gid in each client request.

private:

    Set<Interest *> to_be_scanned;
    Scanner *my_scanner;
    ClientConnection conn;
    Activity a;				// simply declaring it activates timer.
    bool insecure_compat_suggested;

    bool input_msg(const char *msg, int size);

    static bool input_handler(const char *msg, unsigned nbytes, void *closure);
    static void unblock_handler(void *closure);

};

#endif /* !TCP_Client_included */

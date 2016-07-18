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

#include "TCP_Client.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Cred.h"
#include "Event.h"
#include "Interest.h"
#include "Log.h"
#include "Scanner.h"
#include "Listener.h"

//////////////////////////////////////////////////////////////////////////////
//  Construction/destruction

TCP_Client::TCP_Client(in_addr host, int fd, Cred &cr)
    : MxClient(host), cred(cr), my_scanner(NULL),
      conn(fd, input_handler, unblock_handler, this),
      insecure_compat_suggested(false)
{
    assert(fd >= 0);

    // Set client's name.

    char namebuf[20];
    sprintf(namebuf, "client %d", fd);
    name(namebuf);
    Log::debug("new connection from %s", name());
}

TCP_Client::~TCP_Client()
{
}

//////////////////////////////////////////////////////////////////////////////
//  Input

bool
TCP_Client::input_handler(const char *msg, unsigned nbytes, void *closure)
{
    TCP_Client *client = (TCP_Client *) closure;
    if (msg) {
	return client->input_msg(msg, nbytes);
    }
    else
    {   Log::debug("lost connection from %s", client->name());
	delete client;			// NULL msg means connection closed.
        return true;
    }
}

bool
TCP_Client::input_msg(const char *msg, int size)
{
    // Parse the first message.
    // The first message is:
    //	    opcode, request number, uid, gid, file name.

    // If there's an error parsing the message, return false to cause
    // the connection to the misbehaving client to be closed.
    
    bool got_N_with_groups = false;
    char *p = (char *) msg, *q;
    char opcode = *p++;
    Request reqnum = strtol(p, &q, 10);
    if (p == q)
    {
	Log::error("bad message (no request id)");
        return false;
    }
    p = q;
    uid_t uid = strtol(p, &q, 10);
    if (p == q)
    {
	Log::error("bad message (no uid)");
        return false;
    }
    p = q;
    gid_t gid = strtol(p, &q, 10);
    if (p == q)
    {
	Log::error("bad message (no gid)");
        return false;
    }
    p = q;

    // Advance past the space
    p++;
    
    char filename[PATH_MAX + 1];
    int i;
    for (i = 0; *p; i++)
    {   if (i >= PATH_MAX)
	{   Log::error("%s path name too long (%d chars)", name(), i);
	    return false;
	}
	filename[i] = *p++;
    }
    filename[i] = '\0';
    if (i > 0 && filename[i - 1] == '\n')
	filename[i - 1] = '\0';		// strip the trailing newline
    p++;

    // Parse the second message, if any.
    // The second message is:
    //	    ngroups, group, group, group ...

    //  This is a raging kludge.  got_N_with_groups is how a client says
    //  "create a UNIX domain socket for local communication with me."
    //  You guessed that, right?
    //
    //  The reason it's done this way, rather than by having fam listen
    //  for connections from local clients on a unix domain socket in addition
    //  to the internet domain socket, is that when fam is started by inetd,
    //  the first client would have to connect on the internet domain socket
    //  anyway to make inetd exec fam.
    if ((opcode == 'N') && (p < msg + size - 1)) got_N_with_groups = true;

    //  If no Cred is set on the connection, that means we trust the uid
    //  & gids supplied in the message.
    Cred msg_cred = cred;
    if (!msg_cred.is_valid())
    {
        gid_t *grouplist = &gid;
        int ngroups = 1;
        if (p < msg + size - 1)
        {
            ngroups += strtol(p, &p, 10);
            int maxgroups = sysconf(_SC_NGROUPS_MAX);

            if (ngroups > maxgroups)
            {
                Log::info("message contained %i groups, but group list was"
                          " truncated to %i because of _SC_NGROUPS_MAX",
                          ngroups, maxgroups);
                ngroups = maxgroups;
            }
            grouplist = new gid_t[ngroups];
            grouplist[0] = gid;
	    for (int i = 1; i < ngroups; i++)
	    {
	        grouplist[i] = strtol(p, &q, 10);
	        if (p == q)
                {
	            Log::error("bad message (%d additional groups expected, %d found)", ngroups - 1, i);
                    ngroups = i;
                    break;
                }
	        p = q;
	    }
        }

        Cred c(uid, ngroups, grouplist, conn.get_fd());
        msg_cred = c;
        if (grouplist != &gid) delete [] grouplist;
    }

    // Process the message.

    switch (opcode)
    {
    case 'W':				// Monitor File
    {
	Log::debug("%s said: request %d monitor file \"%s\"",
		   name(), reqnum, filename);
	monitor_file(reqnum, filename, msg_cred);
	break;
    }
    case 'M':				// Monitor Directory
	Log::debug("%s said: request %d monitor dir \"%s\"",
		   name(), reqnum, filename);
	monitor_dir(reqnum, filename, msg_cred);
	break;

    case 'C':				// Cancel
	Log::debug("%s said: cancel request %d", name(), reqnum);
	cancel(reqnum);
	break;

    case 'S':				// Suspend
	Log::debug("%s said: suspend request %d", name(), reqnum);
	MxClient::suspend(reqnum);
	break;

    case 'U':				// Resume
	Log::debug("%s said: resume request %d", name(), reqnum);
	MxClient::resume(reqnum);
	break;

    case 'N':				// Client Name
	Log::debug("%s said: %s is %s, and %s a unix domain socket",
                   name(), name(), filename,
                   (got_N_with_groups ? "wants" : "doesn't want"));
	if (*filename && strcmp(filename, "test"))
	    name(filename);		// set this client's name

        //  Check to see whether this client wants to switch to a unix
        //  domain socket.  Create it owned by the uid the client says
        //  they're running as.
        if (got_N_with_groups) Listener::create_local_client(*this, uid);

	break;

    //
    //  Ignore these obsolete messages.
    //
    case 'D':
    case 'V':
    case 'E':
	break;

    default:
	Log::error("%s said unknown request '%c' ('\\%3o')",
		   name(), opcode, opcode & 0377);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//  Output

void
TCP_Client::unblock_handler(void *closure)
{
    TCP_Client *client = (TCP_Client *) closure;

    //  Continue scanner, if any.

    Scanner *scanner = client->my_scanner;
    if (scanner)
    {	if (scanner->done())
	    client->my_scanner = NULL;
	else
	    return;
    }

    //  After scanner has run, scan more interests.

    Interest *ip;
    while (client->ready_for_events() && (ip = client->to_be_scanned.first()))
    {   client->to_be_scanned.remove(ip);
	ip->scan();
    }

    //  Enable input if all enqueued work is done.

    if (client->ready_for_events())
	client->conn.ready_for_input(true);
}

bool
TCP_Client::ready_for_events()
{
    return conn.ready_for_output();
}

void
TCP_Client::enqueue_for_scan(Interest *ip)
{
    if (!to_be_scanned.size())
	conn.ready_for_input(false);
    to_be_scanned.insert(ip);
}

void
TCP_Client::dequeue_from_scan(Interest *ip)
{
    to_be_scanned.remove(ip);
    if (!to_be_scanned.size())
	conn.ready_for_input(true);
}

void
TCP_Client::enqueue_scanner(Scanner *sp)
{
    assert(!my_scanner);
    my_scanner = sp;
    conn.ready_for_input(false);
}

void
TCP_Client::post_event(const Event& event, Request request, const char *path)
{
    conn.send_event(event, request, path);
    Log::debug("sent event to %s: request %d \"%s\" %s",
	       name(), request, path, event.name());
}

//////////////////////////////////////////////////////////////////////////////
//  Random kludges

//  If we're not running in insecure_compatibility mode,
//  and we haven't already logged a message for this connection,
//  put a message in the syslog saying "this client was denied...
//  try insecure_compat mode."
void
TCP_Client::suggest_insecure_compat(const char *path)
{
    if (insecure_compat_suggested) return;

    //  if cred isn't valid it could be a remote connection.
    if ((cred.is_valid()) &&
        (!Cred::insecure_compat_enabled()) &&
        (cred.uid() == Cred::get_untrusted_uid()))
    {
        //  XXX add a config option to turn off this message!
        Log::log(Log::INFO, "Client \"%s\", whose requests are being serviced "
                 "as uid %d, was denied access on %s.  If this client might "
                 "be running as uid %d because it failed authentication, you "
                 "might disable authentication.  See the fam(1M) man page.",
                 name(), cred.uid(), path, cred.uid());
        insecure_compat_suggested = true;
    }
}

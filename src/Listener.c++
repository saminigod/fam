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

#include "Listener.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>  // for inet_ntoa
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/clnt.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <errno.h>

#include "Log.h"
#include "LocalClient.h"
#include "Scheduler.h"
#include "Cred.h"
#include "BTree.h"

#if !(HAVE_BINDRESVPORT_PROTO)
extern "C" int bindresvport(int sd, struct sockaddr_in *);
#endif

struct NegotiatingClient
{
    NegotiatingClient(int fd, uid_t u, struct sockaddr_un *s);
    int sock;
    uid_t uid;
    struct sockaddr_un sun;
};

BTree<int, NegotiatingClient *> negotiating_clients;

static void cleanup_negotiation(void *closure);

Listener::Listener(bool sbi, bool lo, unsigned long p, unsigned long v)
: program(p),
  version(v),
  rendezvous_fd(-1),
  started_by_inetd(sbi),
  _ugly_sock(-1),
  local_only(lo)
{
    if (started_by_inetd)
    {
	//  Portmapper already knows about
	//  us, so just wait for the requests to start rolling in.

	set_rendezvous_fd(RENDEZVOUS_FD);

	dirty_ugly_hack();
    }
    else
    {
	//  Need to register with portmapper.
	//  Unless we're debugging, fork and close all descriptors.

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{   Log::perror("can't create TCP/IP socket for rendezvous");
	    exit(1);
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_only ? htonl(INADDR_LOOPBACK) : 0;
	addr.sin_port = htons(0);
	if (bindresvport(sock, &addr) < 0)
	{
	    Log::perror("can't bind to reserved port");
	    exit(1);
	}
	if (listen(sock, 1) < 0)
	{
	    Log::perror("can't listen for rendezvous");
	    exit(1);
	}
	(void) pmap_unset(program, version);
	if (!pmap_set(program, version, IPPROTO_TCP, ntohs(addr.sin_port)))
	{
	    Log::error("can't register with portmapper.");
	    exit(1);
	}
	set_rendezvous_fd(sock);
    }
}

Listener::~Listener()
{
    if (rendezvous_fd >= 0)
    {   if (!started_by_inetd)
	    pmap_unset(program, version);
	int rc = close(rendezvous_fd);
	if (rc < 0)
	    Log::perror("close rendezvous port(%d)", rendezvous_fd);
	else
	    Log::debug("fam service closed");
	(void) Scheduler::remove_read_handler(rendezvous_fd);
    }
    if (_ugly_sock >= 0)
    {   (void) close(_ugly_sock);
	(void) unlink("/tmp/.fam_socket");
    }
}

void
Listener::set_rendezvous_fd(int newfd)
{
    if (rendezvous_fd >= 0)
    {   (void) Scheduler::remove_read_handler(rendezvous_fd);
	(void) close(rendezvous_fd);
    }
    rendezvous_fd = newfd;
    if (rendezvous_fd >= 0)
    {   (void) Scheduler::install_read_handler(rendezvous_fd,
					       accept_client, this);
	Log::debug("listening for clients on descriptor %d", rendezvous_fd);
    }
}

void
Listener::accept_client(int rendezvous_fd, void *closure)
{
    Listener *listener = (Listener *)closure;

    // Get the new socket.

    sockaddr_in addr;
    socklen_t addrlen = sizeof addr;
    int client_fd = accept(rendezvous_fd, (struct sockaddr *) &addr, &addrlen);
    if (client_fd < 0)
    {
	Log::perror("failed to accept new client");
	return;
    }

    if (ntohl(addr.sin_addr.s_addr) == INADDR_LOOPBACK)
    {
        Log::debug("client fd %d is local/untrusted.", client_fd);
	// Client is local.

        Cred cred = Cred::get_cred_for_untrusted_conn(client_fd);
	new TCP_Client(addr.sin_addr, client_fd, cred);
        // We don't need a reference to this object.  The constructor
        // takes care of registering it with the Scheduler.
    }
    else
    {
        assert(!listener->local_only);

	// Check that client is superuser.
        Cred cred;
	if (ntohs(addr.sin_port) >= IPPORT_RESERVED)
	{
            //  Client isn't connecting on a privileged port, so we will
            //  ignore who they say they are, and treat them as our
            //  untrusted-user.
            cred = Cred::get_cred_for_untrusted_conn(client_fd);
	}
        Log::debug("client fd %d from %s is remote/%strusted",
                   client_fd, inet_ntoa(addr.sin_addr),
                   cred.is_valid() ? "un" : "");

	new TCP_Client(addr.sin_addr, client_fd, cred);
        // We don't need a reference to this object.  The constructor
        // takes care of registering it with the Scheduler.
    }
}

void
Listener::create_local_client(TCP_Client &inet_client, uid_t uid)
{
    //  This TCP_Client wants to use a UNIX domain socket instead of the
    //  inet socket for communication.  Create the new socket owned by the
    //  requested user and pass the name back to the client.

    //  Unset TMPDIR to ensure that tempnam() works as desired
#ifdef HAVE_UNSETENV
    unsetenv("TMPDIR");
#else
    putenv("TMPDIR=");
#endif

    char *tmpfile = tempnam("/tmp", ".fam");
#ifdef HAVE_SOCKADDR_SUN_LEN
    sockaddr_un sun = { sizeof(sockaddr_un), AF_UNIX, "" };
#else
    sockaddr_un sun = { AF_UNIX, "" };
#endif
    if(strlen(tmpfile) >= (sizeof(sun.sun_path) - 1))
    {
        Log::error("tmpnam() too long for sun_path (%d >= %d)!",
                   strlen(tmpfile), sizeof(sun.sun_path) - 1);
        free(tmpfile);
        return;
    }
    strcpy(sun.sun_path, tmpfile);
    free(tmpfile);

    Cred::SuperUser.become_user();
    int client_sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (client_sock < 0)
    {   Log::perror("localclient socket(PF_UNIX, SOCK_STREAM, 0)");
	return;
    }

    Log::debug("client %s said uid %d; creating %s",
               inet_client.name(), uid, sun.sun_path);

    unlink(sun.sun_path);
    if (bind(client_sock, (sockaddr *) &sun, sizeof sun) != 0)
    {   Log::perror("localclient bind");
	close(client_sock);
        return;
    }

    if (chmod(sun.sun_path, 0600) != 0)
    {   Log::perror("localclient chmod");
	close(client_sock);
        return;
    }

    if (chown(sun.sun_path, uid, (gid_t)-1) != 0)
    {   Log::perror("localclient chown");
	close(client_sock);
        return;
    }

    //  Since we're going to start listening on this socket, set a task
    //  to clean it up if we don't receive a connection within 60 seconds.
    NegotiatingClient *nc = new NegotiatingClient(client_sock, uid, &sun);
    negotiating_clients.insert(client_sock, nc);
    timeval nto;
    gettimeofday(&nto, NULL);
    nto.tv_sec += 60;  //  XXX that should be configurable
    Scheduler::install_onetime_task(nto, cleanup_negotiation, nc);

    if (listen(client_sock, 1) != 0)
    {   Log::perror("localclient listen");
	close(client_sock);
        return;
    }

    Scheduler::install_read_handler(client_sock, accept_localclient, NULL);
    Log::debug("listening for requests for uid %d on descriptor %d (%s)",
               uid, client_sock, sun.sun_path);

    inet_client.send_sockaddr_un(sun);
}

void
Listener::accept_localclient(int ofd, void *)
{
    NegotiatingClient *nc = negotiating_clients.find(ofd);
    assert(nc);

    // Get the new socket.

#ifdef HAVE_SOCKADDR_SUN_LEN
    struct sockaddr_un sun = { sizeof(sockaddr_un), AF_UNIX, "" };
#else
    struct sockaddr_un sun = { AF_UNIX, "" };
#endif
    socklen_t sunlen = sizeof(sun);
    int client_fd = accept(ofd, (struct sockaddr *) &sun, &sunlen);
    if (client_fd < 0)
    {
	Log::perror("failed to accept new client");
	return;
    }

    //  Keep the scheduler from helpfully cleaning this up.
    Scheduler::remove_onetime_task(cleanup_negotiation, nc);

    Log::debug("client fd %d is local/trusted (socket %s, uid %d).",
               client_fd, nc->sun.sun_path, nc->uid);
    Cred cred(nc->uid, client_fd);
    new LocalClient(client_fd, &(nc->sun), cred);
    // We don't need a reference to this object.  The constructor
    // takes care of registering it with the Scheduler.

    //  Stop listening for new connections on that socket.
    Scheduler::remove_read_handler(ofd);
    close(ofd);

    //  This client is no longer negotiating, so remove it from the list
    //  and delete it.
    negotiating_clients.remove(ofd);
    delete nc;
}

//  Here's the problem.  If the network is stopped and restarted,
//  inetd and portmapper are killed and new ones are launched in their
//  place.  Since they have no idea that fam is already running, inetd
//  opens a new TCP/IP socket and registers it with the new portmapper.
//  Meanwhile, the old fam has /dev/imon open, and any new fam launched
//  by the new inetd won't be able to use imon.  The old fam can't just
//  quit; it has all the state of all its existing clients to contend with.

//  So here's the dirty, ugly, hack solution.  The first fam to run is
//  the master.  The master opens the UNIX domain socket,
//  /tmp/.fam_socket, and listens for connections from slaves.  Each
//  subsequent fam is a slave.  It knows it's a slave because it can
//  connect to the master.  The slave passes its rendezvous descriptor
//  (the one that fam clients will connect to) through to the master
//  using the access rights mechanism of Unix domain sockets.  The
//  master receives the new descriptor and starts accepting clients on
//  it.  Meanwhile, the slave blocks, waiting for the master to close
//  the connection on /tmp/.fam_socket.  That happens when the master
//  exits.

//  This master/slave stuff has nothing to do with fam forwarding
//  requests to fams on other hosts.  That's called client/server
//  fams.

//  Why not just kill fam when we kill the rest of the network
//  services?  Because too many programs need it.  The desktop, the
//  desktop sysadmin tools, MediaMail...  None of those should go down
//  when the network goes down, and none of them are designed to
//  handle fam dying.

void
Listener::dirty_ugly_hack()
{
#ifdef HAVE_SOCKADDR_SUN_LEN
    static sockaddr_un sun = { sizeof (sockaddr_un), AF_UNIX, "/tmp/.fam_socket" };
#else
    static sockaddr_un sun = { AF_UNIX, "/tmp/.fam_socket" };
#endif

    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {   Log::perror("socket(PF_UNIX, SOCK_STREAM, 0)");
	exit(1);
    }
    
    struct stat sb;
    if (lstat(sun.sun_path, &sb) == 0 &&
	sb.st_uid == 0 && S_ISSOCK(sb.st_mode))
    {
	if (connect(sock, (sockaddr *) &sun, sizeof sun) == 0)
	{   
	    // Another fam is listening to /tmp/.fam_socket.
	    // Pass our rendezvous fd to the other fam and
	    // sleep until that fam exits.

	    int rights[1] = { rendezvous_fd };
	    msghdr msg = { NULL, 0, NULL, 0, (caddr_t) rights, sizeof rights };
	    if (sendmsg(sock, &msg, 0) < 0)
	    {   Log::perror("sendmsg");
		exit(1);
	    }
	    Log::debug("fam (process %d) enslaved to master fam", getpid());
	    char data[1];
	    int nb = read(sock, &data, sizeof data);
	    assert(nb == 0);
	    exit(0);
	}
	else
	    Log::debug("could not enslave myself: %m");
    }

    //  We were unable to connect to another fam.
    //	We'll create our own dirty ugly hack socket and accept connections.

    (void) unlink(sun.sun_path);
    if (bind(sock, (sockaddr *) &sun, sizeof sun) != 0)
    {   Log::perror("bind");
	exit(1);
    }

    if (chmod(sun.sun_path, 0700) != 0)
    {   Log::perror("chmod");
	exit(1);
    }

    if (listen(sock, 1) != 0)
    {   Log::perror("listen");
	exit(1);
    }

    (void) Scheduler::install_read_handler(sock, accept_ugly_hack, this);
    _ugly_sock = sock;
    Log::debug("fam (process %d) is master fam.", getpid());
}

void
Listener::accept_ugly_hack(int ugly, void *closure)
{
    Listener *listener = (Listener *) closure;
    assert(ugly == listener->_ugly_sock);
    
    //  Accept a new ugly connection.

    struct sockaddr_un sun;
    socklen_t sunlen = sizeof sun;
    int sock = accept(ugly, (struct sockaddr *)(&sun), &sunlen);
    if (sock < 0)
    {   Log::perror("accept");
	return;
    }

    (void) Scheduler::install_read_handler(sock, read_ugly_hack, listener);
}

void
Listener::read_ugly_hack(int sock, void *closure)
{
    Listener *listener = (Listener *) closure;

    int rights[1] = { -1 };
    msghdr msg = { NULL, 0, NULL, 0, (caddr_t) rights, sizeof rights };
    if (recvmsg(sock, &msg, 0) >= 0)
    {
	Log::debug("master fam (process %d) has new slave", getpid());

	assert(rights[0] >= 0);
	listener->set_rendezvous_fd(rights[0]);

	//  Turn the inactivity timeout all the way down.  We want to
	//  clean up this dirty ugly hack as soon as possible after
        //  the last Activity is destroyed.
	Activity::timeout(0);

	// Forget socket -- it will be closed on exit.
	// Slave is blocked waiting for socket to close, so slave
	// will exit when master exits.

	(void) Scheduler::remove_read_handler(sock);
    }
    else
    {	Log::perror("recvmsg");
	(void) Scheduler::remove_read_handler(sock);
	(void) close(sock);
    }
}

NegotiatingClient::NegotiatingClient(int fd, uid_t u, struct sockaddr_un *sunp)
    : sock(fd), uid(u)
{
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, sunp->sun_path);
}

static void
cleanup_negotiation(void *closure)
{
    NegotiatingClient *nc = (NegotiatingClient *)closure;

    Log::debug("cleaning up incomplete negotiation for client %d", nc->sock);

    //  Remove client from list
    negotiating_clients.remove(nc->sock);

    //  Remove the read handler & close the socket
    Scheduler::remove_read_handler(nc->sock);
    close(nc->sock);
    nc->sock = -1;

    //  Remove the temp file
    uid_t preveuid = geteuid();
    if (preveuid) seteuid(0);
    seteuid(nc->uid);
    unlink(nc->sun.sun_path);
    if (nc->uid) seteuid(0);
    seteuid(preveuid);

    delete nc;
}


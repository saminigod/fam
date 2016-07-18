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

#include "RPC_TCP_Connector.h"

#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>  // for rresvport

#include "Log.h"
#include "Scheduler.h"
#include "Cred.h"  // for Cred::SuperUser

RPC_TCP_Connector::RPC_TCP_Connector(unsigned long p,
                                     unsigned long v,
                                     unsigned long host,
				     ConnectHandler ch,
                                     void *cl)
    : state(IDLE), sockfd(-1), program(p), version(v),
      connect_handler(ch), closure(cl)
{
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = host;
}

RPC_TCP_Connector::~RPC_TCP_Connector()
{
    deactivate();
}

//////////////////////////////////////////////////////////////////////////////

void
RPC_TCP_Connector::activate()
{
    assert(state == IDLE);
    state = PMAPPING;
    address.sin_port = htons(PMAPPORT);
    retry_interval = INITIAL_RETRY_INTERVAL;
    try_to_connect();
}

void
RPC_TCP_Connector::deactivate()
{
    if (sockfd >= 0)
    {   (void) Scheduler::remove_write_handler(sockfd);
	(void) close(sockfd);
	sockfd = -1;
    }
    if (state == PAUSING)
	(void) Scheduler::remove_onetime_task(retry_task, this);
    state = IDLE;
}

//////////////////////////////////////////////////////////////////////////////

void
RPC_TCP_Connector::try_to_connect()
{
    assert(sockfd == -1);
    assert(state == PMAPPING || state == CONNECTING);

    Cred::SuperUser.become_user();  //  So we can have a privileged port.
    int lport = IPPORT_RESERVED - 1;
    int fd = rresvport(&lport);
    if (fd < 0)
    {   Log::perror("rresvport");
	try_again();
	return;
    }
    int yes = 1;
    int rc = ioctl(fd, FIONBIO, &yes);
    if (rc < 0)
    {   Log::perror("FIONBIO");
        deactivate();
	return;
    }
    rc = connect(fd, (const sockaddr *)&address, sizeof address);
    if (rc == 0)
    {   sockfd = fd;
	write_handler(fd, this);
    }
    else if (errno == EINPROGRESS)
    {   (void) Scheduler::install_write_handler(fd, write_handler, this);
	sockfd = fd;
    }
    else
    {   Log::perror("connect");
	(void) close(fd);
	try_again();
    }
}

void
RPC_TCP_Connector::write_handler(int fd, void *closure)
{
    (void) Scheduler::remove_write_handler(fd);
    RPC_TCP_Connector *conn = (RPC_TCP_Connector *) closure;
    assert(fd == conn->sockfd);
    int rc = connect(fd, (const sockaddr *)(&conn->address), sizeof conn->address);
    if (rc < 0 && errno != EISCONN)
    {
	Log::perror("connect");
	(void) close(conn->sockfd);
	conn->sockfd = -1;
	conn->try_again();
	return;
    }
    switch (conn->state)
    {
    case PMAPPING:
    {
	//  We have connected with portmapper; make a PMAP_GETPORT
	//  call.

	sockaddr_in addr;
	addr.sin_port = htons(PMAPPORT);
	CLIENT *client = clnttcp_create(&addr, PMAPPROG, PMAPVERS, &fd,
					RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	clnt_stat rc = (clnt_stat) ~RPC_SUCCESS;
	unsigned short port = 0;
	if (client)
	{   struct pmap pmp = { conn->program, conn->version, IPPROTO_TCP, 0 };
	    timeval timeout = { PMAP_TIMEOUT, 0 };
	    rc = CLNT_CALL(client, PMAPPROC_GETPORT, (xdrproc_t) xdr_pmap,
			   (caddr_t) &pmp, (xdrproc_t) xdr_u_short, (caddr_t) &port, timeout);
	    if (rc != RPC_SUCCESS)
		Log::info("Portmapper call failed: %s", clnt_sperrno(rc));
	    CLNT_DESTROY(client);
	}
	else
	    Log::info("Couldn't create RPC TCP/IP client: %m");
	(void) close(fd);	
	conn->sockfd = -1;
	if (rc == RPC_SUCCESS && port != 0)
	{   conn->state = CONNECTING;
	    conn->address.sin_port = htons(port);
	    conn->try_to_connect();
	}
	else
	    conn->try_again();
	break;
    }
    case CONNECTING:

	conn->state = IDLE;
	conn->sockfd = -1;
	(*conn->connect_handler)(fd, conn->closure);
	break;

    default:

	int unknown_connector_state = 0; assert(unknown_connector_state);
	break;
    }
}

//////////////////////////////////////////////////////////////////////////////

//  Implement an exponential falloff.  Start trying once a second,
//  slow to once every 1024 seconds (~17 minutes).  Time required by
//  each connection attempt is added in, so if other host is down and
//  TCP has a two minute timeout, we start by polling every 2:01, and
//  slow to every 19:05.

void
RPC_TCP_Connector::try_again()
{
    assert(state == PMAPPING || state == CONNECTING);
    state = PAUSING;

    timeval next_time;
    (void) gettimeofday(&next_time, NULL);
    next_time.tv_sec += retry_interval;
    if (retry_interval < MAX_RETRY_INTERVAL)
	retry_interval *= 2;

    Scheduler::install_onetime_task(next_time, retry_task, this);
}

void
RPC_TCP_Connector::retry_task(void *closure)
{
    RPC_TCP_Connector *conn = (RPC_TCP_Connector *) closure;
    assert(conn->state == PAUSING);
    conn->state = PMAPPING;
    conn->address.sin_port = htons(PMAPPORT);
    conn->try_to_connect();
}

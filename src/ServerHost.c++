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

#include "ServerHost.h"

#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "FileSystem.h"
#include "Event.h"
#include "Listener.h"
#include "Log.h"
#include "Pollster.h"
#include "Scheduler.h"
#include "ServerConnection.h"

///////////////////////////////////////////////////////////////////////////////
//  Construction/Destruction

ServerHost::ServerHost(const hostent& hent)
    : refcount(0), connector(Listener::FAMPROG, Listener::FAMVERS,
		((in_addr *) hent.h_addr)->s_addr, connect_handler, this),
      connection(NULL), unique_request(1), deferred_scans(NULL), last(NULL),
      min_time(0)
{
    // Save first component of full hostname.

    char *p = strchr(hent.h_name, '.');
    unsigned int nchar;
    if (p)
	nchar = p - hent.h_name;
    else
	nchar = strlen(hent.h_name);

    myname = new char[nchar + 1];
    (void) strncpy(myname, hent.h_name, nchar);
    myname[nchar] = '\0';
}

ServerHost::~ServerHost()
{
    assert(!active());
    Scheduler::remove_onetime_task(deferred_scan_task, this);
    if (is_connected())
    {	delete connection;
	Scheduler::remove_onetime_task(timeout_task, this);
    }
    else
	Pollster::forget(this);
    delete [] myname;

    DeferredScan *ds = deferred_scans, *dsp;
    while (ds != NULL)
    {
        dsp = ds;
        ds = ds->next;
        delete dsp;
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Activation.  "Active" means that something on this host needs monitoring.

void
ServerHost::activate()
{
    if (is_connected())
        Scheduler::remove_onetime_task(timeout_task, this);
    else
    {   connector.activate();
	Pollster::watch(this);
    }
}

void
ServerHost::deactivate()
{
    assert(!active());
    if (is_connected())
    {   timeval t;
	(void) gettimeofday(&t, NULL);
	t.tv_sec += Pollster::interval();
	Scheduler::install_onetime_task(t, timeout_task, this);
    }
    else
    {   connector.deactivate();
	Pollster::forget(this);
    }
}

void
ServerHost::timeout_task(void *closure)
{
    ServerHost *host = (ServerHost *) closure;
    assert(host->is_connected());
    Log::debug("disconnecting from server fam@%s "
	       "after %d seconds of inactivity",
	       host->name(), Pollster::interval());
    delete host->connection;
    host->connection = NULL;
}

//////////////////////////////////////////////////////////////////////////////
//  Connection.  Both connection and disconnection are async. events.

void
ServerHost::connect_handler(int fd, void *closure)
{
    ServerHost *host = (ServerHost *) closure;
    assert(host->active());
    assert(!host->is_connected());
    host->connection = new ServerConnection(fd, event_handler,
					    disconnect_handler, host);
    Pollster::forget(host);

    //  Tell the server's fam who we are.

    char myname[MAXHOSTNAMELEN + 11];
    (void) strcpy(myname, "client fam@");
    int rc = gethostname(myname + 11, sizeof myname - 11);
    assert(rc == 0);
    host->connection->send_name(myname);
    Log::debug("connected to server fam@%s", host->name());

    //  Tell the server's fam about existing requests.

    for (Request r = host->requests.first(); r; r = host->requests.next(r))
    {   ClientInterest *ci = host->requests.find(r);
	char remote_path[PATH_MAX];
	ci->filesystem()->hl_map_path(remote_path, ci->name(), ci->cred());
	host->connection->send_monitor(ci->type(), r, remote_path, ci->cred());
	Log::debug("told server fam@%s: request %d monitor file \"%s\"",
		   host->name(), r, remote_path);
	if (!ci->active())
	    host->send_suspend(r);
    }
}

void
ServerHost::disconnect_handler(void *closure)
{
    ServerHost *host = (ServerHost *) closure;
    Log::debug("lost connection to server fam@%s", host->name());
    if (host->active()) {
        assert(host->is_connected());
        delete host->connection;
        host->connection = NULL;
        Pollster::watch(host);
        host->connector.activate();		// Try to reconnect.
    } else {
        // We're in the timeout period waiting to close the
        // connection.  Remove the timeout callback and don't poll
        // this host
        Scheduler::remove_onetime_task(timeout_task, host);
        delete host->connection;
        host->connection = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Input.  For every event of interest, we perform an immediate scan
//  and then we queue a second scan to be done later on.  This is
//  necessary because NFS caches attribute information and we have to
//  wait for old cached info to be out of date.
//
//  For "Changed" events, we save the path because we need it to look
//  up a DirEntry.  We don't save the result of the first lookup
//  because result may become invalid while we're sleeping.
//
//  There is no way to avoid the deferred scan short of adding a
//  hook in the kernel to invalidate an entry in the attribute cache.

void
ServerHost::event_handler(const Event* event, Request r,
					  const char *path, void *closure)
{
    ServerHost *host = (ServerHost *) closure;
    Log::debug("server fam@%s said request %d \"%s\" %s",
	       host->name(), r, path, event->name());

    //  If we care about this event, tell the ClientInterest to scan itself.
    //	Also enqueue a deferred task to rescan later.

    if (*event == Event::Changed || *event == Event::Deleted ||
	*event == Event::Created || *event == Event::Exists)
    {
	ClientInterest *cip = host->requests.find(r);
	if (!cip)
	    return;
	Interest *ip;

	if (*event == Event::Changed || *event == Event::Deleted)
	{   ip = cip->find_name(path);
	    if (!ip)
		return;
	}
	else
	{   ip = cip;
	    path = NULL;
	}
	ip->scan();
	int wait = cip->filesystem()->get_attr_cache_timeout();
	if (wait > 0) {
            host->defer_scan(wait < RETRY_INTERVAL ? 
                             wait : 
                             RETRY_INTERVAL,(int)((wait-1)/RETRY_INTERVAL),
                             r, path);
        }
    }
}

inline
ServerHost::DeferredScan::DeferredScan(int then, int rtrys, Request r, const char *s)
    : when(then), retries(rtrys), next(NULL), myrequest(r)
{
    assert(!s || strlen(s) < sizeof mypath);
    if (s)
    {
	strncpy(mypath, s, (sizeof mypath) - 1);
        mypath[(sizeof mypath) - 1] = '\0';
    }
    else
	mypath[0] = '\0';
}

void
ServerHost::defer_scan(int when, int retries, Request r, const char *path)
{
    timeval t;
    (void) gettimeofday(&t, NULL);
    int then = t.tv_sec + when + 1;

    DeferredScan *ds = new DeferredScan(then, retries, r, path);
    //  In most cases, our new element will go either
    //  at the beginning or end of the list.
    if ((deferred_scans == NULL) || (deferred_scans->when >= then))
    {
        ds->next = deferred_scans;
        deferred_scans = ds;
        if (last == NULL) last = ds;
    }
    else if (last->when <= then)
    {
        last = last->next = ds;
    }
    else
    {
        //  It's in the middle after all.  Put it in the right spot.
        DeferredScan *prev = deferred_scans;
        while ((prev->next != NULL) && (prev->next->when < then))
        {
            prev = prev->next;            
        }
        ds->next = prev->next;
        prev->next = ds;
        //  last still points to an element after ds.
    }

    //  If this new request needs to happen before our previously-first task,
    //  or we didn't have any task scheduled at all, tell the scheduler.
    if (!min_time || then < min_time)
    {	if (min_time)
	    Scheduler::remove_onetime_task(deferred_scan_task, this);
	min_time = then;
	timeval t = { then, 0 };
	Scheduler::install_onetime_task(t, deferred_scan_task, this);
    }
}

void
ServerHost::deferred_scan_task(void *closure)
{
    ServerHost *host = (ServerHost *) closure;
    int& min_time = host->min_time;
    DeferredScan *ds = host->deferred_scans, *dsp;
    assert(ds);
    if(ds == NULL) return;

    bool changed;
    
    timeval t;
    (void) gettimeofday(&t, NULL);
    while ((ds != NULL) && (ds->when <= t.tv_sec))
    {
        ClientInterest *cip = host->requests.find(ds->request());
	if (cip)
	{
            Interest *ip = ds->path() ? cip->find_name(ds->path()) : cip;
	    if (ip)
            {
                Log::debug("Handing a defered scan on: %s (Request %i)",
                           ip->name(), ds->request());
                changed = ip->scan();
                if (!changed) {
                    if (ds->retries > 0) {
                        Log::debug(
                            "Nothing changed, so rescheduling for %i seconds "
                            "(%i retries left)", RETRY_INTERVAL, ds->retries-1);
                        host->defer_scan(RETRY_INTERVAL, ds->retries-1, ds->request(), ds->path());
                    }
                }
            }
        }
        dsp = host->deferred_scans;
        ds = host->deferred_scans = ds->next;
        if (ds == NULL) host->last = NULL;
        delete dsp;
    }
    if (ds != NULL)
    {
        //  We still have some deferred scans which need to happen later.
        min_time = ds->when;
        timeval t = { ds->when, 0 };
        Scheduler::install_onetime_task(t, deferred_scan_task, host);
    }
    else min_time = 0;
}

///////////////////////////////////////////////////////////////////////////////
//  Output

Request
ServerHost::send_monitor(ClientInterest *ci,
			 ClientInterest::Type type,
			 const char *remote_path)
{
    if (!active())
	activate();

    //  Find a unique request number.

    Request r = unique_request++;
    assert(!requests.find(r));

    //  Send the monitor message to the remote fam.

    if (is_connected())
    {   connection->send_monitor(type, r, remote_path, ci->cred());
	Log::debug("told server fam@%s: request %d monitor file \"%s\"",
		   name(), r, remote_path);
    }

    //  Store the request number in the request table.

    requests.insert(r, ci);
    return r;
}

void
ServerHost::send_cancel(Request r)
{
    assert(requests.find(r));
    if (connection)
    {   connection->send_cancel(r);
	Log::debug("told server fam@%s: cancel request %d", name(), r);
    }
    requests.remove(r);
    if (requests.size() == 0)
	deactivate();
}

void
ServerHost::send_suspend(Request r)
{
    if (connection)
    {   connection->send_suspend(r);
	Log::debug("told server fam@%s: suspend request %d", name(), r);
    }
}

void
ServerHost::send_resume(Request r)
{
    if (connection)
    {   connection->send_resume(r);
	Log::debug("told server fam@%s: resume request %d", name(), r);
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Polling

void
ServerHost::poll()
{
    for (Request r = requests.first(); r; r = requests.next(r))
	requests.find(r)->poll();
}

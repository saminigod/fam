//  Copyright (C) 1999-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "Interest.h"

#include <sys/param.h>
#include <sys/sysmacros.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_IRIX_XTAB_VERIFICATION
#include <stdio.h>
#include <exportent.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif  //  HAVE_IRIX_XTAB_VERIFICATION

#include "Boolean.h"
#include "Event.h"
#include "FileSystem.h"
#include "IMon.h"
#include "Log.h"
#include "Pollster.h"
#include "timeval.h"

Interest *Interest::hashtable[];
IMon      Interest::imon(imon_handler);
bool      Interest::xtab_verification = true;

Interest::Interest(const char *name, FileSystem *fs, in_addr host, ExportVerification ev)
    : hashlink(NULL),
      myname(strcpy(new char[strlen(name) + 1], name)),
      scan_state(OK),
      cur_exec_state(NOT_EXECUTING),
      old_exec_state(NOT_EXECUTING),
      myhost(host),
      mypath_exported_to_host(ev == NO_VERIFY_EXPORTED)
{
    memset(&old_stat, 0, sizeof(old_stat)); 
    IMon::Status s = IMon::BAD;

    s = imon.express(name, &old_stat);
    if (s != IMon::OK)
    {   int rc = lstat(name, &old_stat);
	if (rc < 0)
	{   Log::info("can't lstat %s", name);
	    memset(&old_stat, 0, sizeof old_stat);
	}
    }

    dev = old_stat.st_dev;
    ino = old_stat.st_ino;

    if (ev == VERIFY_EXPORTED) verify_exported_to_host();

    if (s == IMon::OK) {
        
        if ((exported_to_host()) && (dev || ino))
        {
            // Insert on new chain.
            
            Interest **ipp = hashchain();
            hashlink = *ipp;
            *ipp = this;
        }
        else revoke();
    }
    
    
#if HAVE_STAT_ST_FSTYPE_STRING
    //  Enable low-level monitoring.
    //  The NetWare filesystem is too slow to monitor, so
    //  don't even try.

    if ( !strcmp( (char *) &old_stat.st_fstype, "nwfs")) {
        return;
    }
#endif

    if (exported_to_host()) fs->ll_monitor(this, s == IMon::OK);
}

Interest::~Interest()
{
    Pollster::forget(this);
    revoke();
    delete[] myname;
}

void
Interest::revoke()
{
    //  Traverse our hash chain.  Delete this entry when we find it.
    //  Also check for other entries with same dev/ino.

    if (dev || ino)
    {
	bool found_same = false;
	for (Interest *p, **pp = hashchain(); ((p = *pp) != NULL); )
	    if (p == this)
		*pp = p->hashlink;	// remove this from list
	    else
	    {   if (p->ino == ino && p->dev == dev)
		    found_same = true;
		pp = &p->hashlink;	// move to next element
	    }
	if (!found_same)
	    (void) imon.revoke(name(), dev, ino);
    }
}

bool
Interest::dev_ino(dev_t newdev, ino_t newino)
{
    // Remove from hash chain and revoke imon's interest.

    revoke();

    dev = newdev;
    ino = newino;

    if (newdev || newino)
    {

        // Express interest.
        IMon::Status s = IMon::BAD;
	s = imon.express(name(), NULL);
        if (s != IMon::OK) {
            return true;
        }
        
	// Insert on new chain.

	Interest **ipp = hashchain();
	hashlink = *ipp;
	*ipp = this;
    }
    else {
	hashlink = NULL;
    }
    return false;
}

/* Returns true if file changed since last stat */
bool
Interest::do_stat()
{
    // Consider the case of a Directory changing into a file to be a
    // simple change, and send only a Changed event.

    struct stat status;
    
    int rc = lstat(name(), &status);
    if (rc < 0) {
	if (errno == ETIMEDOUT) {
	    return false;
	}
        memset(&status, 0, sizeof status);
    }

    bool exists = status.st_mode != 0;
    bool did_exist = old_stat.st_mode != 0;
#ifdef HAVE_STAT_ST_CTIM_TV_NSEC
    bool stat_changed = (old_stat.st_ctim.tv_sec != status.st_ctim.tv_sec) ||
                        (old_stat.st_ctim.tv_nsec != status.st_ctim.tv_nsec) ||
                        (old_stat.st_mtim.tv_sec != status.st_mtim.tv_sec) ||
                        (old_stat.st_mtim.tv_nsec != status.st_mtim.tv_nsec) ||
#else
    bool stat_changed = (old_stat.st_ctime != status.st_ctime) ||
                        (old_stat.st_mtime != status.st_mtime) ||
#endif
                        (old_stat.st_mode != status.st_mode) ||
                        (old_stat.st_uid != status.st_uid) ||
                        (old_stat.st_gid != status.st_gid) ||
                        (old_stat.st_size != status.st_size) ||
                        (old_stat.st_ino != status.st_ino);
    old_stat = status;

    //  If dev/ino changed, move this interest to the right hash chain.

    bool keep_polling = false;
    if (status.st_dev != dev || status.st_ino != ino) {
        keep_polling = dev_ino(status.st_dev, status.st_ino);
    }

    if (exists && !did_exist)
    {
        post_event(Event::Created);
        if (!keep_polling) {
            notify_created(this);
        }
    }
    else if (did_exist && !exists)
    {
        post_event(Event::Deleted);
        notify_deleted(this);
    }

    return stat_changed;
}

bool
Interest::do_scan()
{
    bool stat_changed = false;
    if (needs_scan() && active())
    {   needs_scan(false);
	bool did_exist = exists();
        stat_changed = do_stat();
	if (stat_changed && did_exist && exists())
	    post_event(Event::Changed);
	report_exec_state();
    }
    return stat_changed;
}

void
Interest::report_exec_state()
{
    if (old_exec_state != cur_exec_state && active())
    {   post_event(cur_exec_state == EXECUTING ?
			  Event::Executing : Event::Exited);
	old_exec_state = cur_exec_state;
    }
}

void
Interest::imon_handler(dev_t device, ino_t inumber, int event)
{
    assert(device || inumber);

    for (Interest *p = *hashchain(device, inumber), *next = p; p; p = next)
    {	next = p->hashlink;
	if (p->ino == inumber && p->dev == device)
	{   if (event == IMon::EXEC)
	    {   p->cur_exec_state = EXECUTING;
		(void) p->report_exec_state();
	    }
	    else if (event == IMon::EXIT)
	    {   p->cur_exec_state = NOT_EXECUTING;
		(void) p->report_exec_state();
	    }
	    else
	    {   assert(event == IMon::CHANGE);
		p->scan();
	    }
	}
    }
}

void
Interest::enable_xtab_verification(bool enable)
{
#ifdef HAVE_IRIX_XTAB_VERIFICATION
    xtab_verification = enable;
    Log::info("%s xtab verification of remote requests",
              enable ? "Enabling" : "Disabling");
#endif
}

//  This determines whether this Interest falls on a filesystem which is
//  exported to the host from which the request originated, and sets
//  mypath_exported_to_host accordingly.
void
Interest::verify_exported_to_host()
{
#ifdef HAVE_IRIX_XTAB_VERIFICATION

    //  This sets mypath_exported_to_host by checking /etc/xtab and seeing
    //  whether the export entry has an access list; if it does, we see
    //  whether each entry in the list is a netgroup-containing-the-
    //  requesting-host or the-requesting-host-itself.

    if ((!xtab_verification) ||
        (myhost.s_addr == htonl(INADDR_LOOPBACK)))
    {
        mypath_exported_to_host = true;
        return;
    }
    mypath_exported_to_host = false;

    //  Check the xtab for the list of exported filesystems.  If this
    //  Interest is located on a filesystem which has been exported to the
    //  Interest's host, set mypath_exported_to_host to true.

    Log::debug("XTAB: checking requested interest %s, dev/ino %d/%d, "
               "from host %s", name(), dev, ino, inet_ntoa(myhost));

    //  This is a little bogus... if we don't have a dev or ino, we're not
    //  going to find a matching exported filesystem, so let's bail.
    if (!dev && !ino) {
        Log::debug("XTAB: returning false for dev/ino %d/%d", dev, ino);
        return;
    }

    Cred::SuperUser.become_user();  // setexportent fails if you're not root??
    exportent *xent;
    FILE *xtab = setexportent();
    if (xtab == NULL) {
        Log::perror("setexportent");
        return;
    }

    while ((xent = getexportent(xtab)) != NULL) {
        //  See if the Interest falls under this export entry.
        char *xent_path = xent->xent_dirname;
        int xent_pathlen = strlen(xent_path);
        if (xent_path[xent_pathlen - 1] == '/') --xent_pathlen;
        if ((xent_pathlen == 0) ||  //  xent_path was "/"
            ((strncmp(xent_path, name(), xent_pathlen) == 0) &&
             ((name()[xent_pathlen] == '/') ||   //  so /usr doesn't
              (name()[xent_pathlen] == '\0'))))  //  match /usrbooboo
        {
            //  This export entry is somewhere above the requested directory.
            //  If it has the same device number, this is the entry we want.
            struct stat sb;
//need to set cred here?
            if (stat(xent_path, &sb) == -1) {
//only log if errno != EACCES or ENOENT?
                Log::perror("stat(%s)", name());
                continue;
            }
            if (sb.st_dev == dev) break;  // yippee

            //  Are there other cases we need to handle?  Child filesystems
            //  exported -nohide still need to be exported, and we'll keep
            //  going until we find them.
        }
    }
    endexportent(xtab);  //  Note that we're still using memory returned by
                         //  getexportent().

    if (xent == NULL) {
        //  no matching xtab entry.
        Log::info("Found no matching xtab entry for remote request from %s "
                  "for %s", inet_ntoa(myhost), name());
        return;
    }

    Log::debug("XTAB: %s is on xent %s", name(), xent->xent_dirname);

    char *xopt;
    if ((xopt = getexportopt(xent, ACCESS_OPT)) == NULL) {
        //  This is exported to all clients, so we're OK.
        Log::debug("XTAB: no access list for %s, so remote request was "
                   "granted", xent->xent_dirname);
        mypath_exported_to_host = true;
        return;
    }

    hostent *hent = gethostbyaddr(&myhost, sizeof(in_addr), AF_INET);
    if (hent == NULL) {
        Log::perror("gethostbyaddr(%s)", inet_ntoa(myhost));
        return;
    }

    //  This export entry has a list of netgroups and/or hosts which are
    //  allowed access.  Run through the list & see if our host is there.
    char *cs = xopt, *ce;
    while ((!mypath_exported_to_host) && (cs != NULL) && (*cs != '\0')) {
        ce = strchr(cs, ':');
        if (ce != NULL) *ce = '\0';

        //  See if this client is a netgroup containing myhost.
        Log::debug("XTAB: seeing if %s is a netgroup containing host %s",
                   cs, hent->h_name);
        if (innetgr(cs, hent->h_name, NULL, NULL)) {
            mypath_exported_to_host = true;
            break;
        }
        //  See if this client is a netgroup containing one of myhost's aliases.
        for (int ai = 0; hent->h_aliases[ai] != NULL; ++ai) {
            Log::debug("XTAB: seeing if %s is a netgroup containing host "
                       "alias %s", cs, hent->h_aliases[ai]);
            if (innetgr(cs, hent->h_aliases[ai], NULL, NULL)) {
                mypath_exported_to_host = true;
                break;
            }
        }
        if (mypath_exported_to_host) break;

        //  See if this client is a host.
        Log::debug("XTAB: seeing if %s is a host with the address %s", cs,
                   inet_ntoa(myhost));
        hostent *chent = gethostbyname(cs);
        if ((chent != NULL) &&
            (chent->h_addrtype == AF_INET) &&
            chent->h_length == sizeof(in_addr)) {
            for (int i = 0; chent->h_addr_list[i] != NULL; ++i) {
                if (((in_addr *)(chent->h_addr_list[i]))->s_addr ==
                    myhost.s_addr) {
                    //  whew.  what a pain.
                    mypath_exported_to_host = true;
                    break;
                }
            }
        }

        if(ce != NULL) cs = ce + 1;
        else break;
    }

    Log::info("XTAB: %s request from %s to monitor %s",
              mypath_exported_to_host ? "Granted" : "Denied",
              inet_ntoa(myhost), name());
    return;

#else

    //  We don't have xtab verification, so this just says we're OK.
    mypath_exported_to_host = true;

#endif  //  HAVE_IRIX_XTAB_VERIFICATION
}

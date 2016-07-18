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

#include "Cred.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <stdlib.h>
#include <ctype.h>
#include <pwd.h>
#include <errno.h>

#include "Log.h"
#ifdef HAVE_MAC
#include <sys/mac.h>
#include <t6net.h>
#endif

static gid_t SuperUser_groups[1] = { 0 };
const Cred Cred::SuperUser(0, 1, SuperUser_groups, -1);
Cred Cred::untrusted;
const Cred::Implementation *Cred::Implementation::last = NULL;
Cred::Implementation **Cred::impllist;
unsigned Cred::nimpl;
unsigned Cred::nimpl_alloc;
bool Cred::insecure_compat = false;
#ifdef HAVE_MAC
bool Cred::use_mac = true;
#endif

//  This is used by Listener to create a new Cred for each connection whose
//  per-request UIDs will be ignored.  (The uid could either be the untrusted
//  uid, or the uid which the client has been authenticated as.)  It's also
//  used by Cred::set_untrusted_user() to create the untrusted Cred.
Cred::Cred(uid_t u, int sockfd)
{
    unsigned int nAddlGroups = sysconf(_SC_NGROUPS_MAX);
    struct passwd *pwd;
    gid_t *addlGroups = new gid_t[nAddlGroups];
    gid_t primary_group;
    if ((pwd = getpwuid(u)) != NULL)
    {
        primary_group = pwd->pw_gid;
#if HAVE_GETGRMEMBER
        nAddlGroups = getgrmember(pwd->pw_name, addlGroups, nAddlGroups, 0);
        if (nAddlGroups == -1)
        {
            Log::info("getgrmember(%s, ...) failed: %s",
			pwd->pw_name, strerror(errno));
            nAddlGroups = 0;
        }
#else
        group *gbp;
        unsigned int ii = 0;
        setgrent();
        while ((ii < nAddlGroups) && ((gbp = getgrent()) != NULL))
        {
            //  See if our user's name is in the list of group members.
            for (int i = 0; gbp->gr_mem[i] != NULL; ++i)
            {
                if (!strcmp(pwd->pw_name, gbp->gr_mem[i]))
                {
                    addlGroups[ii++] = gbp->gr_gid;
                    break;
                }
            }
        }
        endgrent();
        nAddlGroups = ii;
#endif  //  HAVE_GETGRMEMBER
    }
    else
    {
        nAddlGroups = 0;
        primary_group = untrusted.is_valid() ? untrusted.gid() : (gid_t) -1;
        Log::info("Warning: uid %i is unknown to this system, so "
                  "that user will be given the gid of the untrusted user: %i",
                  u, primary_group);
                  
    }
    mac_t mac = NULL;
#ifdef HAVE_MAC
    if ((use_mac) && (sockfd != -1))
    {
        if(tsix_get_mac(sockfd, &mac) != 0)
        {
            Log::error("tsix_get_mac failed for client fd %d", sockfd);
            exit(1);
        }
    }
#endif
    new_impl(u, primary_group, nAddlGroups, addlGroups, mac);
    delete [] addlGroups;
}

//  This is used by TCP_Client to create a new Cred for each request from
//  a trusted remote fam.  The MAC label is not handled correctly; the
//  remote fam's is being used, rather than the remote client's.
Cred::Cred(uid_t u, unsigned int ng, const gid_t *gs, int sockfd)
{
    mac_t mac = NULL;
#ifdef HAVE_MAC
    if (use_mac) tsix_get_mac(sockfd, &mac); 
#endif
    // The first gid in the group array should be considered the
    // primary group id, and the remaining groups are additional
    new_impl(u, gs[0], ng-1, gs+1, mac);
}

//  This is used by Cred::get_cred_for_untrusted_conn() to create a new
//  Cred for each unauthenticated connection.  Before the MAC stuff was
//  added, we could just use the untrusted Cred, but now each untrusted can
//  have their own MAC label.
Cred::Cred(int sockfd)
{
#ifdef HAVE_MAC
    if (use_mac)
    {
        mac_t mac = NULL;
        if(tsix_get_mac(sockfd, &mac) != 0)
        {
            //  what to do in this case?  Proceed without a good MAC label?
            Log::error("tsix_get_mac failed for client fd %d", sockfd);
        }
        //  this is broken: needs to test for null untrusted.p
        else if (mac_equal(mac, untrusted.p->mac) == 0)
	{
            new_impl(untrusted.p->myuid, untrusted.p->mygid,
                     untrusted.p->nAddlGroups, untrusted.p->AddlGroups, mac);
            return;
        }
	else mac_free(mac);  // because we'll share the Implementation.
    }
#endif
    //  If we're here, either we're not using MAC or the socket's MAC label
    //  matches untrusted's, so we want to share untrusted's Implementation.
    p = untrusted.p;
    if (p != NULL) p->refcount++;
}

//  This builds an invalid cred which shouldn't be used until it's assigned
//  the value of a good cred.  It's used for the uninitialized untrusted cred,
//  and for the Creds which are created per-connection by
//  Listener::accept_client.
Cred::Cred()
{
    p = NULL;
}

Cred::Cred(const Cred& that)
{
    p = that.p;
    if (p != NULL) p->refcount++;
}

Cred::~Cred()
{
    if (p && (!--p->refcount)) drop(p);
}

Cred&
Cred::operator = (const Cred& that)
{
    if (this != &that)
    {   if ((p != NULL) && (!--p->refcount))
	    drop(p);
	p = that.p;
	if (p != NULL) p->refcount++;
    }
    return *this;
}

void
Cred::add(Implementation *np)
{
    if (nimpl >= nimpl_alloc)
    {   nimpl_alloc = nimpl_alloc * 3 / 2 + 3;
	Implementation **nl = new Implementation *[nimpl_alloc];
	for (int i = 0; i < nimpl; i++)
	    nl[i] = impllist[i];
	delete [] impllist;
	impllist = nl;
    }
    impllist[nimpl++] = np;
}

void
Cred::drop(Implementation *dp)
{
    assert(!dp->refcount);
    for (Implementation **pp = impllist, **end = pp + nimpl; pp < end; pp++)
	if (*pp == dp)
	{   *pp = *--end;
	    assert(nimpl);
	    --nimpl;
	    break;
	}
    delete dp;

    if (!nimpl)
    {   delete[] impllist;
	impllist = NULL;
	nimpl_alloc = 0;
    }
}

void
Cred::new_impl(uid_t u, gid_t g, unsigned int ng, const gid_t *gs, mac_t mac)
{
    for (Implementation **pp = impllist, **end = pp + nimpl; pp < end; pp++)
	if ((*pp)->equal(u, g, ng, gs, mac))
	{   (*pp)->refcount++;
	    p = *pp;
#ifdef HAVE_MAC
            if (mac != NULL) mac_free(mac);
#endif
	    return;
	}

    p = new Implementation(u, g, ng, gs, mac);
    add(p);
}

void
Cred::set_untrusted_user(const char *name)
{
    //  The only time this should get called is at startup, when we're
    //  handling command-line or config-file options, and before any
    //  requests are accepted.
    assert(!untrusted.is_valid());

    //  The untrusted user is never used if we're running in
    //  insecure_compat mode.
    if (insecure_compat) return;

    //  First see if we were passed a uid.
    const char *p = name;
    while (isdigit(*p)) ++p;
    if((*p == '\0') && (p != name))  // need at least one character!
    {
        uid_t uid = atoi(name);
        struct passwd *pwd = getpwuid(uid);
        if(pwd == NULL)
        {
            Log::error("Fatal misconfiguration: attempted to use unknown uid "
                       "\"%i\" for untrusted-user", uid);
            exit(1);
        }
        Cred tmpcred(uid, -1);
        untrusted = tmpcred;
        Log::debug("Setting untrusted-user to \"%s\" (uid: %d, gid: %d)",
                   pwd->pw_name, pwd->pw_uid, pwd->pw_gid);
        return;
    }

    //  Looks like we were passed a user name.
    struct passwd *pwd = getpwnam(name);
    if(pwd == NULL)
    {
        Log::error("Fatal misconfiguration: attempted to use unknown user "
                   "name \"%s\" for untrusted-user", name);
        exit(1);
    }
    Log::debug("Setting untrusted-user to \"%s\" (uid: %d, gid: %d)",
               name, pwd->pw_uid, pwd->pw_gid);
    Cred tmpcred(pwd->pw_uid, -1);
    untrusted = tmpcred;
}

Cred
Cred::get_cred_for_untrusted_conn(int sockfd)
{
    //  An invalid Cred on a connection means we'll trust the Creds supplied
    //  by the connection.  In the case where insecure_compat is enabled, we
    //  always want to return an invalid Cred.  As it happens, untrusted is
    //  always invalid when insecure_compat is enabled, so we just use it.
    //  If insecure compat isn't enabled, we want to return a valid untrusted
    //  Cred.
    assert(untrusted.is_valid() || insecure_compat);
    return insecure_compat ? untrusted : Cred(sockfd);
}

void
Cred::disable_mac()
{
#ifdef HAVE_MAC
    use_mac = false;
    Log::audit(true, "running with MAC disabled, so all client requests will "
                     "use fam's MAC label.");
#endif
}

void
Cred::enable_insecure_compat()
{
    insecure_compat = true;
    Log::audit(true, "running in insecure compatibility mode");
    Log::info("running in insecure compatibility mode");
}

///////////////////////////////////////////////////////////////////////////////

Cred::Implementation::Implementation(uid_t u, gid_t g,
                                     unsigned int ng, const gid_t *gs,
                                     mac_t m)
    : refcount(1), myuid(u), mygid(g), nAddlGroups(ng)
{
#ifdef HAVE_MAC
    mac = NULL;
    if (use_mac) mac = m;
    else if (m != NULL) mac_free(m);
#endif
    AddlGroups = new gid_t[ng];

    for (int i = 0; i < nAddlGroups; i++)
	AddlGroups[i] = gs[i];

    if (nAddlGroups == 0) {
        addlGroupsStr = new char[1];
        *addlGroupsStr = '\0';
    }
    else {
        // The format is: <number of addl groups> <gid1> <gid2> ... <gidn>
        
        // Assume that the each num and accompanying space (or null
        // character in the case of the last one) will be <= 11 chars.
        addlGroupsStr = new char[11*(nAddlGroups + 1)];
        char * p = addlGroupsStr;
        p += snprintf(p, 10, "%d", nAddlGroups);
        for (int i = 0; i < nAddlGroups; i++)
        {
            p += snprintf(p, 11, " %d", AddlGroups[i]);
        }
    }
}

Cred::Implementation::~Implementation()
{
    if (this == last)
	SuperUser.become_user();
    delete [] AddlGroups;
    delete [] addlGroupsStr;
#ifdef HAVE_MAC
    if (mac != NULL) mac_free(mac);
#endif
}

bool
Cred::Implementation::equal(uid_t u, gid_t g, unsigned int ng,
                            const gid_t *gs, mac_t mac) const
{
    if ((u != myuid) || (g != mygid)) {
        return false;
    }
#ifdef HAVE_MAC
    if ((use_mac) && (mac_equal(this->mac, mac) == 0)) return -1;
#endif
    return addl_groups_equal(ng, gs);
}

bool
Cred::Implementation::addl_groups_equal(unsigned int ng, const gid_t *gs) const
{
    if (ng != nAddlGroups) {
        return false;
    }
    for (int i = 0; i < nAddlGroups; i++)
	if (AddlGroups[i] != gs[i])
	    return false;
    return true;
}

// This function returns a string representation of the additional
// groups, as required by ServerConnection::send_monitor().
const char *
Cred::Implementation::getAddlGroupsString() const {
    return addlGroupsStr;
}

void
Cred::Implementation::become_user() const
{
    // If we're becoming the same user we currently are, then we can
    // just skip everything.
    if (this == last)
	return;

    uid_t current_uid = last ? last->myuid : 0;
    if (current_uid != 0) {
        /* Temporarily set the effective uid to root's uid
         * so that we have permission to call setgroups, etc.
         * This assumes, of course, that we were started as root,
         * so that we have permission to do this.
         */
        Log::debug("Setting euid to 0");
        if (seteuid(0) != 0)
        {
            Log::perror("failed to set 0 uid");
            exit(1);
        }
    }

    // We need to set the primary group and additional groups before
    // setting our euid because non-root users don't have permission
    // to change the groups.
    if (!last || !addl_groups_equal(last->nAddlGroups, last->AddlGroups)) {
	if (setgroups(nAddlGroups, AddlGroups) != 0) {
            Log::perror("failed to set groups");
            exit(1);
	} else if (Log::get_level() == Log::DEBUG) {
            if (nAddlGroups == 0) {
                Log::debug("Setting groups to: (none)");
            } else {
                // The groupStr variable is almost what we want, but
                // it has the number of groups prepended.  So just
                // skip over that.
                char * p = strchr(addlGroupsStr, ' ');
                Log::debug("Setting groups to: %s", p+1);
            }
        }
    } else {
        Log::debug("Skipping setting groups, because they're already correct");
    }
    
    if (!last || (mygid != last->mygid)) {
        if (setegid(mygid)) {
            Log::perror("failed to set gid %d", mygid);
            exit(1);
        } else {
            Log::debug("Setting egid to %i", mygid);
        }
    } else {
        Log::debug("Skipping setting egid, because it's already correct");
    }
    
    

    if (myuid != 0) { // We can skip this if we're becoming root, as
                      // we either entered this function as root, or
                      // set our euid to root above
        if (seteuid(myuid)) {
            Log::perror("failed to set uid %d", myuid);
            exit(1);
        } else {
            Log::debug("Setting euid to %i", myuid);
        }
    } else {
        if (current_uid != 0) {
            // We already logged a message above
        } else {
            Log::debug("Skipping setting euid, because it's already 0");
        }
    }
    

#ifdef HAVE_MAC
    if (use_mac)
    {
        if (mac_set_proc(mac) != 0)
        {
            Log::perror("become_user() failed to set MAC label for uid %d", myuid);
        }
    }
#endif
    last = this;
}


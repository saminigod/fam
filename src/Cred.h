//  Copyright (C) 1999-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef Cred_included
#define Cred_included

#include <sys/param.h>
#include <sys/types.h>
#include <stddef.h>

#include "Boolean.h"

#ifdef HAVE_MAC
#include <sys/mac.h>
#else
//  This typedef could be a really stupid idea.  It's just so that we
//  don't need to #ifdef the few methods that take a mac_t argument.  If
//  HAVE_MAC isn't defined, we should be ignoring those arguments anyway.
typedef void * mac_t;
#endif

//  Cred is short for Credentials, which is what NFS calls the
//  structure that holds the user's uids and gids.
//
//  A user of a Cred can get its uid and gid, and get an ASCII string
//  for its group list.  A user can also pass the message
//  become_user() which will change the process's effective uid and
//  gid and group list to match the Cred's.  If the new IDs are the
//  same as the current IDs, become_user() doesn't do any system
//  calls.
//
//  The Cred itself is simply a pointer to the Implementation.  The
//  Implementation is reference counted, so when the last Cred
//  pointing to one is destroyed, the Implementation is destroyed too.
//
//  Implementations are shared.  There is currently a linked list of
//  all Implementations, and that list is searched whenever a new Cred
//  is created.  A faster lookup method would be good...

class Cred {

public:

    Cred();
    Cred(const Cred &that);
    Cred(uid_t, int sockfd);
    Cred(uid_t, unsigned int ngroups, const gid_t *, int sockfd);
    Cred& operator = (const Cred& that);
    ~Cred();

    bool is_valid() const               { return p != NULL; }
    uid_t uid() const			{ return p->uid(); }
    uid_t gid() const			{ return p->gid(); }

    // The caller must not delete the memory returned
    const char * getAddlGroupsString() const {return p->getAddlGroupsString();}

    void become_user() const		{ p->become_user(); }

    static const Cred SuperUser;

    static void set_untrusted_user(const char *name);
    static uid_t get_untrusted_uid()           { return untrusted.is_valid() ? untrusted.uid() : (uid_t)-1; }
    static Cred get_cred_for_untrusted_conn(int sockfd);
    static void disable_mac();
    static void enable_insecure_compat();
    static bool insecure_compat_enabled() { return insecure_compat; }

private:

    Cred(int sockfd);

    class Implementation {

    public:

	Implementation(uid_t, gid_t, unsigned int, const gid_t *, mac_t);
	~Implementation();
	bool equal(uid_t, gid_t, unsigned int ngroups,
                    const gid_t *, mac_t) const;
	int cmp(uid_t, unsigned ngroups, const gid_t *, mac_t) const;

	uid_t uid() const		{ return myuid; }
	gid_t gid() const		{ return mygid; }
	const char * getAddlGroupsString() const;
	
	void become_user() const;

	unsigned refcount;

        friend class Cred; // so that set_untrusted_user can modify myuid

    private:

	uid_t myuid;
        gid_t mygid;
        unsigned int nAddlGroups;
        gid_t *AddlGroups;
         char * addlGroupsStr;
#ifdef HAVE_MAC
	mac_t mac;
#endif

        bool addl_groups_equal(unsigned int ng, const gid_t *gs) const;

	static const Implementation *last;

    };

    Implementation *p;
    static Cred untrusted;
    static bool insecure_compat;
#ifdef HAVE_MAC
    static bool use_mac;
#endif

    static Implementation **impllist;
    static unsigned nimpl, nimpl_alloc;

    static void add(Implementation *);
    static void drop(Implementation *);

    void new_impl(uid_t, unsigned int, const gid_t *, mac_t);
    void new_impl(uid_t, gid_t, unsigned int, const gid_t *, mac_t);
};

#endif /* !Cred_included */

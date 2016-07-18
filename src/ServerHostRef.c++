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

#include "ServerHostRef.h"

#include <assert.h>
#include <netdb.h>

unsigned int		   ServerHostRef::count;
StringTable<ServerHost *> *ServerHostRef::hosts_by_name;

///////////////////////////////////////////////////////////////////////////////
//  ServerHostRef methods simply maintain refcounts on ServerHosts.

ServerHostRef&
ServerHostRef::operator = (const ServerHostRef& that)
{
    if (this != &that)
    {
	if (p && !--p->refcount)
	    delete p;
	p = that.p;
	if (p)
	    p->refcount++;
    }
    return *this;
}

ServerHostRef::~ServerHostRef()
{
    if (!--p->refcount)
    {
	// Remove from hosts_by_name.

	const char *name;
	for (unsigned i = 0; ((name = hosts_by_name->key(i)) != NULL); )
	    if (hosts_by_name->value(i) == p)
		hosts_by_name->remove(name);
	    else
		i++;

	delete p;
    }

    // When the last reference is gone, delete hosts_by_name.

    if (!--count && hosts_by_name)
    {	assert(!hosts_by_name->size());
	delete hosts_by_name;
	hosts_by_name = NULL;
    }
}

ServerHost *
ServerHostRef::find(const char *name)
{
    //  Create hosts_by_name first time.

    if (!hosts_by_name)
	hosts_by_name = new StringTable<ServerHost *>;

    //  Look in list of existing hosts.

    ServerHost *host = hosts_by_name->find(name);
    if (host)
	return host;

    //  Look up hostname via gethostbyname().

    hostent *hp = gethostbyname(name);
    char *fake_haliases[1];
    in_addr fake_haddr;
    in_addr *fake_haddrlist[2];
    hostent fake_hostent;

    if (!hp)
    {
	//  gethostbyname() failed for some reason (e.g., ypbind
	//  is down or NIS map just changed, ethernet came unplugged,
	//  or something equally tragic).
	//
	//  Fake it by building a hostent with an illegal address.

	fake_haliases[0] = NULL;
	fake_haddr.s_addr = 0;
	fake_haddrlist[0] = &fake_haddr;
	fake_haddrlist[1] = NULL;
	fake_hostent.h_name = (char *) name;
	fake_hostent.h_aliases = fake_haliases;
	fake_hostent.h_addrtype = 0;
	fake_hostent.h_length = sizeof fake_haddr;
	fake_hostent.h_addr_list = (char **) fake_haddrlist;
	hp = &fake_hostent;
    }

    assert(hp->h_length == sizeof (struct in_addr));

    //  Create new host.

    host = new ServerHost(*hp);

    //  Add this host to hosts_by_name.

    hosts_by_name->insert(name, host);
    hosts_by_name->insert(hp->h_name, host);
    for (char *const*p = hp->h_aliases; *p; p++)
	hosts_by_name->insert(*p, host);

    return host;
}

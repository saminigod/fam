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

#include "Client.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

in_addr
Client::LOCALHOST()
{
    in_addr addr;
    addr.s_addr = htonl(INADDR_LOOPBACK);
    return addr;
}

Client::Client(const char *name, in_addr host)
    : myname(name ? strcpy(new char[strlen(name) + 1], name) : NULL),
      myhost(host)
{ }

Client::~Client()
{
    delete [] myname;
}

const char *
Client::name()
{
    return myname ? myname : "unknown";
}

void
Client::name(const char *newname)
{
    delete [] myname;
    myname = newname ? strcpy(new char[strlen(newname) + 1], newname) : NULL;
}


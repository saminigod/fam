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

#ifndef Pollster_included
#define Pollster_included

#include <sys/time.h>
#include <stdlib.h>

#include "Boolean.h"
#include "Set.h"

class Interest;
class ServerHost;

//  The Pollster remembers what needs to be polled, and wakes up every
//  few seconds to poll it.  The Pollster polls Interests and Hosts.
//  Each Host, if it can't connect to remote fam, polls its own Interests.
//
//  The Pollster's polling interval can be set or interrogated.
//  Also, remote polling can be enabled or disabled using enable()
//  or disable() (corresponds to "fam -l").
//
//  When there's nothing to poll, the Pollster turns itself off, so
//  fam can sleep for a long time.
//
//  Pollster is not instantiated; instead, a bunch of static methods
//  implement its interface.

class Pollster {

public:

    static void watch(Interest *);
    static void forget(Interest *);

    static void watch(ServerHost *);
    static void forget(ServerHost *);

    static void interval(unsigned secs)	{ pintvl.tv_sec = (long)secs; }
    static unsigned interval()		{ return pintvl.tv_sec; }
    static void enable()		{ remote_polling_enabled = true; }
    static void disable()		{ remote_polling_enabled = false; }

private:

    // Class Variables

    static bool remote_polling_enabled;
    static timeval pintvl;		// polling interval
    static Set<Interest *> polled_interests;
    static Set<ServerHost *> polled_hosts;
    static bool polling;

    // Private Class Methods

    static void polling_task(void *closure);
    static void start_polling();
    static void stop_polling();

    Pollster();				// Do not instantiate.

};

#endif /* !Pollster_included */

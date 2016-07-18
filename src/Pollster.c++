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

#include "Pollster.h"

#include "Interest.h"
#include "Log.h"
#include "Scheduler.h"
#include "ServerHost.h"

bool		  Pollster::remote_polling_enabled = true;
timeval		  Pollster::pintvl;  //  set when main() calls interval()
Set<Interest *>	  Pollster::polled_interests;
Set<ServerHost *> Pollster::polled_hosts;
bool		  Pollster::polling = false;

void
Pollster::watch(Interest *ip)
{
    polled_interests.insert(ip);

    if (!polling)
	start_polling();
}

void
Pollster::forget(Interest *ip)
{
    int old_size = polled_interests.size();
    (void) polled_interests.remove(ip);
    int new_size = polled_interests.size();

    if (old_size && !new_size && !polled_hosts.size())
	stop_polling();
}

void
Pollster::watch(ServerHost *ip)
{
    if (remote_polling_enabled)
    {   polled_hosts.insert(ip);

	if (!polling)
	    start_polling();
    }
}

void
Pollster::forget(ServerHost *ip)
{
    int old_size = polled_hosts.size();
    (void) polled_hosts.remove(ip);
    int new_size = polled_hosts.size();

    if (old_size && !new_size && !polled_interests.size())
	stop_polling();
}

//////////////////////////////////////////////////////////////////////////////

void
Pollster::start_polling()
{
    assert(!polling);
    assert(polled_interests.size() || polled_hosts.size());
    Log::debug("polling every %d seconds", pintvl.tv_sec);
    (void) Scheduler::install_recurring_task(pintvl, polling_task, NULL);
    polling = true;
}

void
Pollster::stop_polling()
{
    assert(polling);
    assert(!polled_interests.size());
    assert(!polled_hosts.size());
    (void) Scheduler::remove_recurring_task(polling_task, NULL);
    polling = false;
    Log::debug("will stop polling");
}

void
Pollster::polling_task(void *)
{
    timeval t0, t1;
    (void) gettimeofday(&t0, NULL);

    int ni = 0;
    for (Interest *ip = polled_interests.first();
	 ip;
	 ip = polled_interests.next(ip))
    {
	ip->poll();
	ni++;
    }

    int nh = 0;
    if (remote_polling_enabled)
	for (ServerHost *hp = polled_hosts.first();
	     hp;
	     hp = polled_hosts.next(hp))
	{
	    hp->poll();
	    nh++;
	}

    if ((ni || nh) && (Log::get_level() == Log::DEBUG))
    {   (void) gettimeofday(&t1, NULL);
	double t = t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec)/1000000.0;
	Log::debug("polled %d interest%s and %d host%s in %.3f seconds",
		   ni, ni == 1 ? "" : "s",
		   nh, nh == 1 ? "" : "s",
		   t);
    }
}

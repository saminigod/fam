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

#include "Activity.h"

#include <unistd.h>

#include "Log.h"
#include "Scheduler.h"

unsigned Activity::idle_time = default_timeout;
unsigned Activity::count;

Activity::Activity()
{
    if ((count++ == 0) && (idle_time > 0))
	Scheduler::remove_onetime_task(task, NULL);
}

Activity::Activity(const Activity&)
{
    Activity();
}

Activity::~Activity()
{
    if ((--count == 0) && (idle_time > 0))
    {   timeval t;
	(void) gettimeofday(&t, NULL);
	t.tv_sec += idle_time;
	Scheduler::install_onetime_task(t, task, NULL);
    }
}

void
Activity::task(void *)
{
    Log::debug("exiting after %d second%s of inactivity",
	       idle_time, idle_time == 1 ? "" : "s");
    Scheduler::exit();
}

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

#ifndef Activity_included
#define Activity_included

//  The Activity class is used to keep track of when fam is doing
//  something worthwhile, and when it isn't.  Currently, the only
//  worthwhile activity is talking to (non-internal) clients.  So an
//  Activity is enclosed in each TCP_Client.  When the last Activity
//  is destroyed, a death timer is started.  When the death timer
//  expires, fam expires too.
//
//  The duration of the timer can be set (timeout()), but there is
//  no other public interface to Activity.

class Activity {

public:

    enum { default_timeout = 5 };	// five seconds

    Activity();
    Activity(const Activity&);
    ~Activity();

    static void timeout(unsigned t)	{ idle_time = t; }

private:

    static void task(void *);

    static unsigned count;
    static unsigned idle_time;

};

#endif /* !Activity_included */

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

#ifndef Scheduler_included
#define Scheduler_included

#include <sys/time.h>
#include <string.h>

#include "Boolean.h"

//  Scheduler is a class encapsulating a simple event and timer based
//  task scheduler.  Tasks are simply subroutines that are activated
//  by calling them.
//
//  A recurring task will be activated at fixed intervals of real
//  time.  It is unpredictable when the Scheduler will first activate
//  a recurring task.
//
//  A onetime task will be activated at a particular time.
//
//  Events are those defined by select(2) -- file descriptors that are
//  ready to be read, written or checked for exceptional conditions.
//  A handler may be installed to be called for any event on any
//  descriptor.  Only one handler may be installed for each event.
//  The installation and removal routines return the previously
//  installed handler.
//
//  Scheduler has fixed priorities -- write events precede exception
//  events precede read events precede timed tasks.
//
//  USE: There are no instances of Scheduler; all its interface
//  routines are static, and are called, e.g., "Scheduler::loop();"
//
//  RESTRICTIONS: Because we currently need only a single recurring
//  task, multiple recurring tasks are not implemented.

class Scheduler {

public:

    typedef void (*IOHandler)(int fd, void *closure);
    typedef void (*TimedProc)(void *closure);

    //  One-time tasks.

    static void install_onetime_task(const timeval& when,
				     TimedProc, void *closure);
    static void remove_onetime_task(TimedProc, void *closure);

    //  Recurring tasks.

    static void install_recurring_task(const timeval& interval,
				       TimedProc, void *closure);
    static void remove_recurring_task(TimedProc, void *closure);

    //  I/O handlers.

    static IOHandler install_read_handler(int fd, IOHandler, void *closure);
    static IOHandler remove_read_handler(int fd);

    static IOHandler install_write_handler(int fd, IOHandler, void *closure);
    static IOHandler remove_write_handler(int fd);

    //  Mainline code.

    static void select();
    static void exit()			{ running = false; }
    static void loop()			{ running = true;
					  while (running) select(); }

private:

    //  Per-filedescriptor info is the set of three handlers and their
    //  closures.

    struct FDInfo {
	struct FDIOHandler {
	    IOHandler handler;
	    void *closure;
	} read, write;
    };

    //  Per-I/O type info is the file descriptor set passed to select(),
    //  the number of bits set there, and the offset into the FDInfo
    //  for the corresponding I/O type.

    struct IOTypeInfo {
	FDInfo::FDIOHandler FDInfo::* iotype;
	unsigned int nbitsset;		// number of bits set in fds
	fd_set fds;
	IOTypeInfo(FDInfo::FDIOHandler FDInfo::* a_iotype) :
            iotype(a_iotype), nbitsset(0) { FD_ZERO(&fds); }
    };

    struct onetime_task {
	onetime_task *next;
	timeval when;
	Scheduler::TimedProc proc;
	void *closure;
    };

    // I/O event related variables

    static IOTypeInfo read, write;
    static FDInfo *fdinfo;
    static unsigned int nfds;
    static unsigned int nfdinfo_alloc;

    // Timed task related variables

    static unsigned int ntasks;
    static timeval next_task_time;
    static TimedProc recurring_proc;
    static void *recurring_closure;
    static timeval recurring_interval;
    static onetime_task *first_task;
    static bool running;

    // I/O event related functions

    static FDInfo *fd_to_info(int fd);
    static void trim_fdinfo();
    static IOHandler install_io_handler(int fd,
					IOHandler handler, void *closure,
					IOTypeInfo *iotype);
    static IOHandler remove_io_handler(int fd, IOTypeInfo *iotype);
    static void handle_io(const fd_set *fds, FDInfo::FDIOHandler FDInfo::* iotype);

    // Timed task related functions

    static void do_tasks();
    static timeval *calc_timeout();

    Scheduler();			// Never instantiate a Scheduler.

};

#endif /* !Scheduler_included */

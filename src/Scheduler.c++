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

#include "Scheduler.h"

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>

#include "Log.h"
#include "timeval.h"

//  Define a bunch of class-global variables.

Scheduler::IOTypeInfo	 Scheduler::read(&FDInfo::read);
Scheduler::IOTypeInfo	 Scheduler::write(&FDInfo::write);
unsigned int		 Scheduler::nfds;
Scheduler::FDInfo	*Scheduler::fdinfo;
unsigned int		 Scheduler::nfdinfo_alloc;

unsigned int		 Scheduler::ntasks;
Scheduler::TimedProc	 Scheduler::recurring_proc;
void			*Scheduler::recurring_closure;
timeval			 Scheduler::next_task_time;
timeval			 Scheduler::recurring_interval;
Scheduler::onetime_task	*Scheduler::first_task;
bool			 Scheduler::running;


//////////////////////////////////////////////////////////////////////////////
//  One time task code

void
Scheduler::install_onetime_task(const timeval& when,
				TimedProc proc, void *closure)
{
    onetime_task **fp = &first_task;
    while (*fp && (*fp)->when < when)
	fp = &(*fp)->next;
    onetime_task *nt = new onetime_task;
    nt->next = *fp;
    *fp = nt;
    nt->when = when;
    nt->proc = proc;
    nt->closure = closure;
    ntasks++;
}

void
Scheduler::remove_onetime_task(TimedProc proc, void *closure)
{
    onetime_task *p, **pp = &first_task;
    while ((p = *pp) != NULL)
    {
        if (p->proc == proc && p->closure == closure)
	{   *pp = p->next;
	    delete p;
	    ntasks--;
	    break;
	}    
	pp = &p->next;
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Recurring task code

void
Scheduler::install_recurring_task(const timeval& interval,
				  TimedProc proc, void *closure)
{
    timeval now;

    assert(!recurring_proc);
    assert(proc);
    assert(interval.tv_sec >= 0);
    assert(interval.tv_usec >= 0 && interval.tv_usec < 1000000);
    assert(interval.tv_sec || interval.tv_usec);

    recurring_proc = proc;
    recurring_closure = closure;
    recurring_interval = interval;
    ntasks++;
    (void) gettimeofday(&now, NULL);
    next_task_time = now + interval;
}

void
Scheduler::remove_recurring_task(TimedProc proc, void *closure)
{
    assert(proc == recurring_proc);
    assert(closure == recurring_closure);

    recurring_proc = NULL;
    recurring_closure = closure;
    timerclear(&recurring_interval);
    ntasks--;
}

//  do_tasks activates all timer based tasks.  It also sets
//  next_task_time to the absolute time when it should next be
//  invoked.

void
Scheduler::do_tasks()
{
    if (ntasks)
    {
        timeval now;
	(void) gettimeofday(&now, NULL);
	if (recurring_proc && now >= next_task_time)
	{
	    // Time for the next task.

            (*recurring_proc)(recurring_closure);
	    next_task_time += recurring_interval;

	    // If the clock has jumped ahead or we've gotten too far behind,
	    // postpone the next task.

	    if (next_task_time < now)
		next_task_time = now + recurring_interval;
	}
	else
	{   timeval time_left = next_task_time - now;
	    if (recurring_interval < time_left)
	    {
		// More time left than we started with -- clock must have
		// run backward.  Reset next_task_time to sane value.

		next_task_time = now + recurring_interval;
	    }
	}

	while (first_task && first_task->when < now)
	{   TimedProc proc = first_task->proc;
	    void *closure = first_task->closure;
	    remove_onetime_task(proc, closure);
	    (*proc)(closure);
	}
    }
}

//  calc_timeout calculates the timeout to pass to select().
//  It returns:
//		NULL (if no timed tasks exist)
//		zero time (if it's time for the next task)
//		nonzero time (if it's not time yet)

timeval *
Scheduler::calc_timeout()
{
    static timeval sleep_interval;
    
    if (ntasks)
    {
	timeval wake_time;
	if (recurring_proc)
	    wake_time = next_task_time;
	if (!recurring_proc || first_task && first_task->when < wake_time)
	    wake_time = first_task->when;
	timeval now;
	(void) gettimeofday(&now, NULL);
	sleep_interval = wake_time - now;
	if (sleep_interval.tv_sec < 0)
	    timerclear(&sleep_interval);
	return &sleep_interval;
    }
    else
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//  I/O handler code

//  fd_to_info converts a file descriptor to a pointer into the
//  fdinfo array.  On the way, it verifies that the array has
//  been allocated far enough out.

Scheduler::FDInfo *
Scheduler::fd_to_info(int fd)
{
    assert(fd >= 0);
    if (nfds < fd + 1)
    {
	if (nfdinfo_alloc < fd + 1)
	{
	    unsigned newalloc = nfdinfo_alloc * 3 / 2 + 10;
	    if (newalloc < fd + 1)
		newalloc = fd + 1;
	    FDInfo *newinfo = new FDInfo[newalloc];
	    for (unsigned i = 0; i < nfds; i++)
		newinfo[i] = fdinfo[i];
	    delete [] fdinfo;
	    fdinfo = newinfo;
	    nfdinfo_alloc = newalloc;
	}

	// Zero all new fdinfo's.
	memset(&fdinfo[nfds], 0, (fd + 1 - nfds) * sizeof *fdinfo);
	nfds = fd + 1;
    }
    return &fdinfo[fd];
}

//  trim_fdinfo makes the fdinfo array smaller if its last entries
//  aren't being used.  The memory isn't actually freed unless the
//  array is completely zeroed out.

void
Scheduler::trim_fdinfo()
{
    for (FDInfo *fp = &fdinfo[nfds - 1]; nfds > 0; --nfds, --fp)
	if (fp->read.handler || fp->write.handler)
	    break;

    if (!nfds)
    {   delete [] fdinfo;
	fdinfo = NULL;
	nfdinfo_alloc = 0;
    }
}

Scheduler::IOHandler
Scheduler::install_io_handler(int fd, IOHandler handler, void *closure,
			      IOTypeInfo *iotype)
{
    assert(fd >= 0);
    assert(handler);
    FDInfo *fp = fd_to_info(fd);
    IOHandler old_handler = (fp->*(iotype->iotype)).handler;
    (fp->*(iotype->iotype)).handler = handler;
    (fp->*(iotype->iotype)).closure = closure;
    assert(!old_handler || FD_ISSET(fd, &iotype->fds));
    if (!FD_ISSET(fd, &iotype->fds))
    {
        FD_SET(fd, &iotype->fds);
        iotype->nbitsset++;
    }
    return old_handler;
}

Scheduler::IOHandler
Scheduler::remove_io_handler(int fd, IOTypeInfo *iotype)
{
    assert(fd >= 0 && fd < nfds);
    FDInfo *fp = fd_to_info(fd);
    IOHandler old_handler = (fp->*(iotype->iotype)).handler;
    (fp->*(iotype->iotype)).handler = NULL;
    (fp->*(iotype->iotype)).closure = NULL;
    trim_fdinfo();
    assert(old_handler);
    if (FD_ISSET(fd, &iotype->fds))
    {
        FD_CLR(fd, &iotype->fds);
        iotype->nbitsset--;
    }
    return old_handler;
}

Scheduler::IOHandler
Scheduler::install_read_handler(int fd, IOHandler handler, void *closure)
{
    return install_io_handler(fd, handler, closure, &read);
}

Scheduler::IOHandler
Scheduler::remove_read_handler(int fd)
{
    return remove_io_handler(fd, &read);
}

Scheduler::IOHandler
Scheduler::install_write_handler(int fd, IOHandler handler, void *closure)
{
    return install_io_handler(fd, handler, closure, &write);
}

Scheduler::IOHandler
Scheduler::remove_write_handler(int fd)
{
    return remove_io_handler(fd, &write);
}

void
Scheduler::handle_io(const fd_set *fds, FDInfo::FDIOHandler FDInfo::* iotype)
{
    if (fds)
	for (int fd = 0; fd < nfds; fd++)
	    if (FD_ISSET(fd, fds))
	    {   FDInfo *fp = &fdinfo[fd];
		assert(iotype == &FDInfo::read || iotype == &FDInfo::write);
		(fp->*iotype).handler(fd, (fp->*iotype).closure);
		// Remember, handler may move fdinfo array.
	    }
}

// Scheduling priorities defined here: writable descriptors have
// highest priority, followed by exceptionable descriptors, then
// readable descriptors, then timed tasks have the lowest priority.

void
Scheduler::select()
{
    fd_set readfds, writefds;
    readfds = Scheduler::read.fds;
    writefds =  Scheduler::write.fds;
    timeval *timeout   = calc_timeout();

    int status = ::select(nfds, &readfds, &writefds, 0, timeout);

    if (status == -1 && errno != EINTR)
    {   Log::perror("select");		// Oh, no!
	::exit(1);
    }
    if (status > 0)
    {
	// I/O is ready -- find it and do it.

	handle_io( &writefds, &FDInfo::write );
	handle_io(  &readfds, &FDInfo::read  );
    }

    // Check for tasks now.

    do_tasks();
}

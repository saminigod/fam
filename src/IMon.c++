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

#include "IMon.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#if HAVE_IMON
#ifdef __sgi
#include <sys/imon.h>
#else
#include <linux/imon.h>
#endif
#endif

#include <sys/sysmacros.h>
#include <unistd.h>

#include "Interest.h"
#include "Log.h"
#include "Scheduler.h"
#include "alloc.h"

int		   IMon::imonfd = -2;
IMon::EventHandler IMon::ehandler = NULL;

IMon::IMon(EventHandler h)
{
    assert(ehandler == NULL);
    ehandler = h;
}

IMon::~IMon()
{
    if (imonfd >= 0)
    {
	//  Tell the scheduler.

	(void) Scheduler::remove_read_handler(imonfd);

	//  Close the inode monitor device.

	if (close(imonfd) < 0)
	    Log::perror("can't close /dev/imon");
	else
	    Log::debug("closed /dev/imon");
	imonfd = -1;
    }
    ehandler = NULL;
}

bool
IMon::is_active()
{
    if (imonfd == -2)
    {   imonfd = imon_open();
	if (imonfd >= 0)
	{   Log::debug("opened /dev/imon");
	    (void) Scheduler::install_read_handler(imonfd, read_handler, NULL);
	}
    }
    return imonfd >= 0;
}

IMon::Status
IMon::express(const char *name, struct stat *status)
{
    if (!is_active())
	return BAD;

    return imon_express(name, status);
}

IMon::Status
IMon::revoke(const char *name, dev_t dev, ino_t ino)
{
    if (!is_active())
	return BAD;

    return imon_revoke(name, dev, ino);
}

void
IMon::read_handler(int fd, void *)
{
#if HAVE_IMON
    qelem_t readbuf[20];
    int rc = read(fd, readbuf, sizeof readbuf);
    if (rc < 0)
        Log::perror("/dev/imon read");
    else
    {   assert(rc % sizeof (qelem_t) == 0);
	rc /= sizeof (qelem_t);
	for (int i = 0; i < rc; i++)
	    if (readbuf[i].qe_what == IMON_OVER)
		Log::error("imon event queue overflow");
	    else
	    {	dev_t dev = readbuf[i].qe_dev;
		ino_t ino = readbuf[i].qe_inode;
		intmask_t what = readbuf[i].qe_what;
		Log::debug("imon said dev %d/%d, ino %ld changed%s%s%s%s%s%s",
			   major(dev), minor(dev), ino,
			   what & IMON_CONTENT   ? " CONTENT"   : "",
			   what & IMON_ATTRIBUTE ? " ATTRIBUTE" : "",
			   what & IMON_DELETE    ? " DELETE"    : "",
			   what & IMON_EXEC      ? " EXEC"      : "",
			   what & IMON_EXIT      ? " EXIT"      : "",
#ifdef IMON_RENAME
			   what & IMON_RENAME    ? " RENAME"    : "",
#endif
			   ""
		    );
		if (what & IMON_EXEC)
		    (*ehandler)(dev, ino, EXEC);
		if (what & IMON_EXIT)
		    (*ehandler)(dev, ino, EXIT);
		if (what & (IMON_CONTENT | IMON_ATTRIBUTE |
			    IMON_DELETE
#ifdef IMON_RENAME
			    | IMON_RENAME
#endif
		    ))
		    (*ehandler)(dev, ino, CHANGE);
	    }
    }
#else
    //  I forget why/how this happens, but I know it's happened to me
    //  (something about building fam without imon support, but then it
    //  tries and succeeds? in opening /dev/imon?)  Anyway, it shouldn't
    //  do that.  This should be fixed.
    Log::critical("imon event received by fam built without imon support... "
                  "tell fam@oss.sgi.com to fix this!");
    int i_dont_have_IMON = 0;
    assert(i_dont_have_IMON);
#endif  //  HAVE_IMON
}

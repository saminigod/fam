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

#include "Log.h"
#include "IMon.h"

#include <sys/imon.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <unistd.h>


const intmask_t INTEREST_MASK = (IMON_CONTENT | IMON_ATTRIBUTE | IMON_DELETE |
				 IMON_EXEC | IMON_EXIT | IMON_RENAME);

int IMon::imon_open()
{
    int imon = open("/dev/imon", O_RDONLY, 0);
    if (imon == -1) {
	Log::critical("can't open /dev/imon: %m");
    }
    return imon;
}

IMon::Status IMon::imon_express(const char *name, struct stat *status)
{
    interest_t interest = { (char *) name, status, INTEREST_MASK };
    int rc = ioctl(imonfd, IMONIOC_EXPRESS, &interest);
    if (rc < 0)
    {
        if (name[0] == '/') {
            Log::info("IMONIOC_EXPRESS on \"%s\" failed (euid: %i): %m",
                      name, geteuid());
        } else {
            char * cwd = getcwd(0, 256);
            Log::info("IMONIOC_EXPRESS on \"%s\" with cwd \"%s\" failed (euid: %i): %m",
                      name,cwd,geteuid());
            free(cwd);
        }
	return BAD;
    }
    else
    {
	if (status != NULL) {
	    Log::debug("told imon to monitor \"%s\" = dev %d/%d, ino %d", name,
		       major(status->st_dev), minor(status->st_dev),
		       status->st_ino);
        } else {
	    Log::debug("told imon to monitor \"%s\" (but I didn't ask for dev/ino)",
                       name);
        }
	return OK;
    }
}

IMon::Status IMon::imon_revoke(const char *name, dev_t dev, ino_t ino)
{
    static bool can_revokdi = true;
    bool revokdi_failed;

#ifdef HAVE_IMON_REVOKDI
    if (can_revokdi)
    {
	struct revokdi revokdi = { dev, ino, INTEREST_MASK };
	int rc = ioctl(imonfd, IMONIOC_REVOKDI, &revokdi);
	if (rc < 0)
	{   Log::perror("IMONIOC_REVOKDI on \"%s\" failed", name);
	    revokdi_failed = true;
	    if (errno == EINVAL)
		can_revokdi = false;
	}
	else
	    revokdi_failed = false;
    }
#else
    can_revokdi = false;
#endif  //  !HAVE_IMON_REVOKDI

    if (!can_revokdi || revokdi_failed)
    {
	// Try the old revoke ioctl.

	interest_t interest = { (char *) name, NULL, INTEREST_MASK };
	int rc = ioctl(imonfd, IMONIOC_REVOKE, &interest);
	if (rc < 0)
	{   //  Log error at LOG_DEBUG.  IMONIOC_REVOKE fails all the time.
	    Log::debug("IMONIOC_REVOKE on \"%s\" failed: %m", name);
	    return BAD;
	}
    }

    Log::debug("told imon to forget \"%s\"", name);
    return OK;
}

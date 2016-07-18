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
#include "Log.h"

#include <linux/imon.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define DEV_IMON "/dev/imon"
const intmask_t INTEREST_MASK = (IMON_CONTENT | IMON_ATTRIBUTE | IMON_DELETE |
				 IMON_EXEC | IMON_EXIT);

enum OpenStatus { OPEN_OK, OPEN_RETRY, OPEN_FAILED };

static OpenStatus open_device(int& imonfd)
{
    int major;
    bool foundMajor = false;
    char line[100], name[100];

    FILE* dev = fopen("/proc/devices", "r");
    if (!dev) {
	Log::critical("can't open /proc/devices: %m");
	return OPEN_FAILED;
    }
    while (fgets(line, sizeof line, dev) != NULL) {
	if (sscanf(line, "%d %s\n", &major, name) == 2
	    && strcmp(name, "imon") == 0) {
	    foundMajor = true;
	    break;
	}
    }
    fclose(dev);

    if (!foundMajor) {
	return OPEN_RETRY;
    }

    (void)unlink(DEV_IMON);
    if (mknod(DEV_IMON, S_IFCHR | 0600, makedev(major, 0)) == -1) {
	Log::critical("can't create %s: %m", DEV_IMON);
	return OPEN_FAILED;
    }

    int imon = open(DEV_IMON, O_RDONLY | O_NONBLOCK);
    (void)unlink(DEV_IMON);
    if (imon == -1) {
	Log::critical("can't open %s: %m", DEV_IMON);
	return OPEN_FAILED;
    }
    imonfd = imon;
    return OPEN_OK;
}

static void insmod()
{
    pid_t pid = fork();
    if (pid == 0) {
	execl("/sbin/insmod", "insmod", "imon", NULL);
	exit(1);
    }
    if (pid > 0) {
	waitpid(pid, NULL, 0);
    }
}

int IMon::imon_open()
{
    int imon;
    OpenStatus status = open_device(imon);
    if (status == OPEN_RETRY) {
	insmod();
	status = open_device(imon);
    }
    if (status == OPEN_RETRY) {
	Log::critical("can't open %s: imon major number not found in /proc/devices", DEV_IMON);
	return -1;
    }
    if (status != OPEN_OK) {
	Log::critical("can't open %s: %m", DEV_IMON);
	return -1;
    }
    return imon;
}

IMon::Status IMon::imon_express(const char *name, struct stat *status)
{
    famstat_t famstat;
    struct interest interest = { name, &famstat, INTEREST_MASK };
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

    //
    // Check for a race condition; if someone removed or changed the
    // file at the same time that we are expressing interest in it,
    // revoke the interest so we don't get notifications about changes
    // to a recycled inode that we don't otherwise care about.
    //
    struct stat st;
    if (status == NULL) {
	status = &st;
    }
    if (stat(name, status) == -1) {
	Log::perror("stat on \"%s\" failed", name);
	revoke(name, famstat.st_dev, famstat.st_ino);
	return BAD;
    }
    if (status->st_dev != famstat.st_dev
	|| status->st_ino != famstat.st_ino) {
	Log::error("File \"%s\" changed between express and stat",
		   name);
	revoke(name, famstat.st_dev, famstat.st_ino);
	return BAD;
    }	

    Log::debug("told imon to monitor \"%s\" = dev %d/%d, ino %d", name,
	       major(status->st_dev), minor(status->st_dev),
	       status->st_ino);

    return OK;
}

IMon::Status IMon::imon_revoke(const char *name, dev_t dev, ino_t ino)
{
    revoke_t rv = { dev, ino, INTEREST_MASK };
    int rc = ioctl(imonfd, IMONIOC_REVOKE, &rv);
    if (rc < 0) {
	Log::perror("IMONIOC_REVOKE on \"%s\" failed", name);
	return BAD;
    }
    Log::debug("told imon to forget \"%s\"", name);
    return OK;
}

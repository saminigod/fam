//  Copyright (C) 1999 Silicon Graphics, Inc.  All Rights Reserved.
//  
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of version 2.1 of the GNU Lesser General Public License
//  as published by the Free Software Foundation.
//
//  This program is distributed in the hope that it would be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Further, any
//  license provided herein, whether implied or otherwise, is limited to
//  this program in accordance with the express provisions of the GNU Lesser
//  General Public License.  Patent licenses, if any, provided herein do not
//  apply to combinations of this program with other product or programs, or
//  any other product whatsoever. This program is distributed without any
//  warranty that the program is delivered free of the rightful claim of any
//  third person by way of infringement or the like.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write the Free Software Foundation, Inc., 59
//  Temple Place - Suite 330, Boston MA 02111-1307, USA.

#include <sys/types.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <syslog.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>

#include "fam.h"
#include "Client.h"

//#define DEBUG


// Some libserver stuff
#define FAMNUMBER 391002 // This is the official rpc number for fam.
#define FAMNAME "sgi_fam"
#define FAMVERS 2
#define LOCALHOSTNUMBER 0x7f000001 // Internet number for loopback.

// Error variables - XXX currently not dealt with
#define FAM_NUM_ERRORS 1
int FAMErrno;
char *FamErrlist[FAM_NUM_ERRORS];

struct GroupStuff
{
    GroupStuff();
    ~GroupStuff() { delete [] groups; }
    int groupString(char *buf, int buflen);

    gid_t *groups;
    int   ngroups;
};

static int findFreeReqnum();
static int checkRequest(FAMRequest *fr, const char *filename);
static int sendSimpleMessage(char code, FAMConnection *fc, const FAMRequest *fr);

/**************************************************************************
* FAMOpen, FAMClose - opening, closing FAM connections
**************************************************************************/

int FAMOpen2(FAMConnection* fc, const char* appName)
{
    int famnumber = FAMNUMBER, famversion = FAMVERS;
    struct rpcent *fament = getrpcbyname(FAMNAME);
    if(fament != NULL)
    {
        famnumber = fament->r_number;
    }

    //  Try to connect.
    fc->client = new Client(LOCALHOSTNUMBER, famnumber, famversion);
    fc->fd = ((Client *)fc->client)->getSock();
    if (fc->fd < 0) {
	delete (Client *)fc->client;
        fc->client = NULL;
	return(-1);
    }

    // Send App name 
    if (appName) {
	char msg[200];
	snprintf(msg, sizeof(msg), "N0 %d %d %s\n", geteuid(), getegid(), appName);
	((Client *)fc->client)->writeToServer(msg, strlen(msg)+1);
    }

    return(0);
}

int FAMOpen(FAMConnection* fc)
{
    return FAMOpen2(fc, NULL);
}

int FAMClose(FAMConnection* fc)
{
    delete (Client *)fc->client;
    return(0);
}



/**************************************************************************
* FAMMonitorDirectory, FAMMonitorFile - monitor directory or file
**************************************************************************/

#define ILLEGAL_REQUEST -1

static int FAMMonitor(FAMConnection *fc, const char *filename, FAMRequest* fr,
	       void* userData, int code)
{

    if (!filename || filename[0] != '/')
	return -1;

    int reqnum = fr->reqnum;
    Client *client = (Client *)fc->client;
    // store user data if necessary
    if(userData != NULL) client->storeUserData(reqnum, userData);

    GroupStuff groups;
    char msg[MSGBUFSIZ];
    int msgLen;
    snprintf(msg, MSGBUFSIZ, "%c%d %d %d %s\n", code, reqnum, geteuid(),
                             groups.groups[0], filename);
    msgLen = strlen(msg) + 1;  // include terminating \0 in msg
    if(groups.ngroups > 1)
    {
        msgLen += groups.groupString(msg + msgLen, MSGBUFSIZ - msgLen);
        ++msgLen;  //  include terminating \0
    }

    // Send to FAM
    client->writeToServer(msg, msgLen);
    return(0);
}
int FAMMonitorDirectory(FAMConnection *fc,
			const char *filename,
			FAMRequest* fr,
			void* userData)
{
    if(checkRequest(fr, filename) != 0) return -1;

#ifdef DEBUG
printf("FAMMonitorDirectory filename:%s, reqnum:%d, ud:%d\n", filename, fr->reqnum, (int) userData);
#endif
    return FAMMonitor(fc, filename, fr, userData, 'M');
}
int FAMMonitorDirectory2(FAMConnection *fc,
			 const char *filename,
			 FAMRequest* fr)
{
    return FAMMonitor(fc, filename, fr, NULL, 'M');
}

int FAMMonitorFile(FAMConnection *fc,
		   const char *filename,
		   FAMRequest* fr,
		   void* userData)
{
    if(checkRequest(fr, filename) != 0) return -1;

#ifdef DEBUG
printf("FAMMonitorFile filename:%s, reqnum:%d, ud:%d\n", filename, fr->reqnum, (int) userData);
#endif
    return FAMMonitor(fc, filename, fr, userData, 'W');
}
int FAMMonitorFile2(FAMConnection *fc,
		    const char *filename,
		    FAMRequest* fr)
{
    return FAMMonitor(fc, filename, fr, NULL, 'W');
}


int 
FAMMonitorCollection(FAMConnection* fc,
		     const char* filename,
		     FAMRequest* fr,
		     void* userData,
		     int depth,
		     const char* mask)
{
    if(checkRequest(fr, filename) != 0) return -1;

    Client *client = (Client *)fc->client;

    // store user data if necessary
    if (userData) client->storeUserData(fr->reqnum, userData);

    GroupStuff groups;
    char msg[MSGBUFSIZ];
    int msgLen;
    snprintf(msg, MSGBUFSIZ, "F%d %d %d %s\n", fr->reqnum, geteuid(),
                             groups.groups[0], filename);
    msgLen = strlen(msg) + 1;  // include terminating \0 in msg

    if(groups.ngroups > 1)
    {
        msgLen += groups.groupString(msg + msgLen, MSGBUFSIZ - msgLen);
    }

    snprintf(msg + msgLen, MSGBUFSIZ - msgLen, "0 %d %s\n", depth, mask);
    ++msgLen;  //  include terminating \0

    // Send to FAM
    client->writeToServer(msg, msgLen);
    return(0);
}


/**************************************************************************
* FAMSuspendMonitor, FAMResumeMonitor - suspend FAM monitoring
**************************************************************************/

int FAMSuspendMonitor(FAMConnection *fc, const FAMRequest *fr)
{
    return sendSimpleMessage('S', fc, fr);
}

int FAMResumeMonitor(FAMConnection *fc, const FAMRequest *fr)
{
    return sendSimpleMessage('U', fc, fr);
}



/**************************************************************************
* FAMCancelMonitor - cancel FAM monitoring
**************************************************************************/

int FAMCancelMonitor(FAMConnection *fc, const FAMRequest* fr)
{
#ifdef DEBUG
printf("FAMCancelMonitor reqnum:%d\n", fr->reqnum);
#endif
    // Remove from user Data array
    // Actually, we will do this when we receive the ack back from fam
    // FAMFreeRequest(fr->reqnum);

    return sendSimpleMessage('C', fc, fr);
}


/**************************************************************************
* FAMNextEvent() - find the next fam event
* FAMPending() - return if events are ready yet
**************************************************************************/

//  FAMNextEvent tries to read one complete message into the input buffer.
//  If it's successful, it parses the message and returns 1.
//  Otherwise (EOF or errors), it returns -1.

int FAMNextEvent(FAMConnection* fc, FAMEvent* fe)
{
    fe->fc = fc;
    return ((Client *)fc->client)->nextEvent(fe);
}

//  FAMPending tries to read one complete message into the input buffer.
//  If it's successful, then it returns true.  Also, if it reads EOF, it
//  returns true.

int FAMPending(FAMConnection* fc)
{
    return ((Client *)fc->client)->eventPending();
}



/**************************************************************************
* FAMDebugLevel() - doesn't do anything
**************************************************************************/
int FAMDebugLevel(FAMConnection*, int)
{
    return(1);
}



/**************************************************************************
* Support Functions
**************************************************************************/

static int
findFreeReqnum()
{
    static int reqnum=1;
    return(reqnum++);
}

static int
checkRequest(FAMRequest *fr, const char *filename)
{
    // Fill out request structure
    int reqnum;
    if ((reqnum = findFreeReqnum()) == ILLEGAL_REQUEST) {
	return(-1);
    }
    fr->reqnum = reqnum;

    // Check path length of file
    if (strlen(filename) > MAXPATHLEN) {
	syslog(LOG_ALERT, "path too long\n");
	errno = ENAMETOOLONG;
	return (-1);
    }
    return 0;
}

static int
sendSimpleMessage(char code, FAMConnection *fc, const FAMRequest *fr)
{
    // Create FAM String
    char msg[MSGBUFSIZ];
    snprintf(msg, MSGBUFSIZ, "%c%d %d %d\n", code, fr->reqnum, geteuid(), getegid());

    // Send to FAM
    ((Client *)fc->client)->writeToServer(msg, strlen(msg)+1);
    return(0);
}

GroupStuff::GroupStuff()
{
    ngroups = sysconf(_SC_NGROUPS_MAX);
    groups = new gid_t[ngroups];
    ngroups = getgroups(ngroups, groups);
}

int
GroupStuff::groupString(char *buf, int buflen)
{
    if(ngroups < 2) return 0;
    const int GROUP_STRLEN = 8;  // max length of " %d" output, we hope
    if(buflen < (ngroups * GROUP_STRLEN)) return 0;
    //XXX that should create some error condition

    char *p = buf;
    snprintf(p, GROUP_STRLEN, "%d", ngroups - 1);
    p += strlen(p);
    for(int i = 1; i < ngroups; ++i)  //  group[0] doesn't go in string
    {
        snprintf(p, GROUP_STRLEN, " %d", groups[i]);
        p += strlen(p);
    }
    *p = '\0';

    return p - buf;
}


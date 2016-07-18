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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <string.h>  // for memset
#include <ctype.h>
#include <syslog.h>
#include <errno.h>

#include <iostream.h>

#include "fam.h"
#include "Client.h"

static void getword(const char *p, u_int32_t *l);

Client::Client(long host, unsigned int prog, int vers)
    : sock(0), haveCompleteEvent(false), userData(NULL), endExist(NULL),
      inend(inbuf)
{
    struct sockaddr_in sin;

    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(host);
    //  This is set below instead.
    //sin.sin_port = htons(pmap_getport(&sin, prog, vers, IPPROTO_TCP));

    //
    //  We'll run through the list of pmaps ourselves instead of calling
    //  pmap_getport, because pmap_getport will give you the port for a
    //  version 1 service even when you ask for version 2, and we really
    //  need to know which version of fam we're talking to.  (Isn't there
    //  an easier way to do that?)
    //
    pmaplist *pl = pmap_getmaps(&sin);  //  this is leaked; see note below loop
    unsigned long bestvers = 0;
    for (pmaplist *plp = pl; plp != NULL; plp = plp->pml_next)
    {
        if ((plp->pml_map.pm_prog == prog) &&
            (plp->pml_map.pm_prot == IPPROTO_TCP))
        {
            if (plp->pml_map.pm_vers > bestvers)
            {
                bestvers = plp->pml_map.pm_vers;
                sin.sin_port = htons((unsigned short)(plp->pml_map.pm_port));
                if (bestvers == vers)
                {
                    break;
                }
            }
        }
    }
    //  We can't call clnt_freeres because we don't have the client, and
    //  Purify says this xdr_free gives a bunch of UMR & UMW's, so we'll
    //  just leak it.  This sucks!  (call CLNT_CALL(client, PMAPPROC_DUMP, ...
    //  ourselves?)
    //xdr_free((xdrproc_t)xdr_pmaplist, &pl);

    if(sin.sin_port == 0)
    {
        //  Couldn't get port for rpc call.
        sock = -1;
        return;
    }
    int insock = socket(PF_INET, SOCK_STREAM, 0);
    if (insock < 0)
    {
        sock = -2;
        return;
    }
    if (connect(insock, (const struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        close(insock);
        sock = -3;
        return;
    }

    //  If we're version 1, we're going to use the inet socket for
    //  communicating with fam, so we're done.
    if (bestvers == 1)
    {
        sock = insock;
        return;
    }

    //  If we're still here, we want to request a unix domain socket from
    //  fam, and use that for communicating.  The "N" message with a group
    //  list (sort of) is how we tell fam we're version 2.
    char msg[200];
    snprintf(msg + sizeof(u_int32_t), sizeof(msg) - sizeof(u_int32_t),
             "N0 %d %d sockmeister%c0\n", geteuid(), getegid(), '\0');
    int len = strlen(msg + sizeof(u_int32_t)) + 1;
    len += (strlen(msg + sizeof(u_int32_t) + len) + 1);
    u_int32_t nmsglen = htonl(len);
    memcpy(msg, &nmsglen, sizeof(u_int32_t));
    len += sizeof(u_int32_t);
    if (write(insock, msg, len) != len)
    {
        close(insock);
        sock = -6;
        return;
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof sun);
    sun.sun_family = AF_UNIX;

    //  We will block here, waiting for response from fam.
    unsigned int nread = 0;
    char inbuf[sizeof(sun.sun_path)];
    while(nread < sizeof(u_int32_t))
    {
        int rv = read(insock, inbuf + nread, sizeof(u_int32_t) - nread);
        if (rv <= 0)
        {
            close(insock);
            sock = -7;
            return;
        }
        nread += rv;
    }
    u_int32_t mlen;
    memcpy(&mlen, inbuf, sizeof(mlen));
    mlen = ntohl(mlen);
    if (mlen >= sizeof(sun.sun_path))
    {
        close(insock);
        sock = -8;
        return;
    }

    nread = 0;
    while (nread < mlen)
    {
        int rv = read(insock, inbuf + nread, mlen - nread);
        if (rv <= 0)
        {
            close(insock);
            sock = -9;
            return;
        }
        nread += rv;
    }
    strncpy(sun.sun_path, inbuf, mlen);
    sun.sun_path[mlen] = '\0';

    //  When we connected to the inet socket and told fam our UID, fam
    //  created a new UNIX domain socket and sent its name to us.  Now
    //  let's connect on that socket and return.
    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        close(insock);
        sock = -10;
        return;
    }
    if (connect(sock, (const struct sockaddr *)&sun, sizeof(sun)) < 0)
    {
        close(sock);
        close(insock);
        sock = -11;
        return;
    }
    //  all done on the inet sock
    close(insock);
}

Client::~Client()
{
    if(sock >= 0) close(sock);
    if(userData != NULL) delete userData;
    if(endExist != NULL) delete endExist;
}

int
Client::writeToServer(char *buf, int nbytes)
{
    if (!connected()) return -1;
    char msgHeader[sizeof(u_int32_t)];
    nbytes = htonl(nbytes);
    memcpy(msgHeader, &nbytes, sizeof(u_int32_t));
    nbytes = ntohl(nbytes);
    if(write(sock, msgHeader, sizeof(u_int32_t)) != sizeof(u_int32_t))
    {
        return -1;
    }
    return write(sock, buf, nbytes);
}

int
Client::eventPending()
{
    if (readEvent(false) < 0) return 1;  //  EOF or error
    return haveCompleteEvent ? 1 : 0;
}

int
Client::nextEvent(FAMEvent *fe)
{
    if (!connected()) return -1;
    if ((!haveCompleteEvent) && (readEvent(true) < 0))
    {
        //  EOF now
        return -1;
    }
    //  readEvent(true) blocks until we have a complete event or EOF.

    u_int32_t msglen;
    getword(inbuf, &msglen);

    char *p = inbuf + sizeof (u_int32_t), *q;
    int limit;
    // 
    char code, changeInfo[100];
    int reqnum;

    code = *p++;
    reqnum = strtol(p, &q, 10);
    if (p == q)
    {
        croakConnection("Couldn't find reqnum in message!");
        return -1;
    }
    //  XXX it would be nice to make sure reqnum is valid, but we only store
    //  it if they gave us user data or if it needs an endexists message
    fe->fr.reqnum = reqnum;
    fe->userdata = getUserData(reqnum);
    p = q;
    p++;
    if (code == 'c')
    {
        q = changeInfo;
        limit = sizeof(changeInfo);
        while ((*p != '\0') && (!isspace(*p)) && (--limit)) *q++ = *p++;
        if (!limit)
        {
            char msg[100];
            snprintf(msg, sizeof(msg),
                     "change info too long! (%d max)", sizeof(changeInfo));
            croakConnection(msg);
            return -1;
        }
        *q = '\0';
        while (isspace(*p)) ++p;
    }

    q = fe->filename;
    limit = PATH_MAX;
    while ((*p != '\0') && (*p != '\n') && (--limit)) *q++ = *p++;
    if (!limit)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "path too long! (%d max)", PATH_MAX);
        croakConnection(msg);
        return -1;
    }
    *q = '\0';

    switch (code) {
	case 'c': // change
	    fe->code = FAMChanged;
	    break;
	case 'A': // delete
	    fe->code = FAMDeleted;
	    break;
	case 'X': // start execute
	    fe->code = FAMStartExecuting;
	    break;
	case 'Q': // quit execute
	    fe->code = FAMStopExecuting;
	    break;
	case 'F':
            fe->code = getEndExist(reqnum) ? FAMCreated : FAMExists;
	    break;
	case 'G':
	    // XXX we should be able to free the user data here
	    freeRequest(reqnum);
	    fe->code = FAMAcknowledge;
	    break;
	case 'e': // new - XXX what about exists ?
            fe->code = getEndExist(reqnum) ? FAMCreated : FAMExists;
	    break;
	case 'P':
	    fe->code = FAMEndExist;
            storeEndExist(reqnum);
	    break;
	default:
            snprintf(changeInfo, sizeof(changeInfo),
                     "unrecognized code '%c'!", code);
            croakConnection(changeInfo);
            return -1;
    }
#ifdef DEBUG
printf("\nFAM received %s  ", msg);
printf("translated to event code:%d, reqnum:%d, ud:%d, filename:<%s>\n",
	fe->code, reqnum, fe->userdata, fe->filename);
#endif

    //  Now that we've copied the contents out of this message, slide the
    //  contents of the buffer over.  Crude, but easier than letting the
    //  buffer wrap.
    msglen += sizeof(u_int32_t);  //  include the size now; less math
    memmove(inbuf, inbuf + msglen, inend - inbuf - msglen);
    inend -= msglen;
    checkBufferForEvent();

    return 1;
}

void
Client::storeUserData(int reqnum, void *p)
{
    if(p == NULL) return;
    if(userData == NULL) userData = new BTree<int, void *>();
    userData->insert(reqnum, p);
}

void *
Client::getUserData(int reqnum)
{
    if(userData == NULL) return NULL;
    return userData->find(reqnum);
}

void
Client::storeEndExist(int reqnum)
{
    //  It's actually kind of dumb to use a BTree for this, since the only
    //  value we ever store will be "true", but access should be fast.
    //  I'm not sure whether or not we'll tend to have a lot of requests
    //  stored at a time, though.
    if(endExist == NULL) endExist = new BTree<int, bool>();
    endExist->insert(reqnum, true);
}

bool
Client::getEndExist(int reqnum)
{
    if(endExist == NULL) return false;
    return endExist->find(reqnum);
}

void
Client::freeRequest(int reqnum)
{
    if(userData != NULL) userData->remove(reqnum);
    if(endExist != NULL) endExist->remove(reqnum);
}

int
Client::readEvent(bool block)
{
    if (!connected()) return -1;
    if (haveCompleteEvent) return 0;

    if (!block)
    {
        fd_set tfds;
        struct timeval tv = { 0, 0 };
        FD_ZERO(&tfds);
        FD_SET(sock, &tfds);
        if (select(sock + 1, &tfds, NULL, NULL, &tv) < 1) return 0;
    }

    do
    {
        int rc = read(sock, inend, MSGBUFSIZ - (inend - inbuf));
        if (rc <= 0) return -1;  //  EOF now
        inend += rc;
        checkBufferForEvent();
    } while (block && !haveCompleteEvent);

    return 0;
}

void
Client::checkBufferForEvent()
{
    if (!connected()) return;
    haveCompleteEvent = false;
    u_int32_t msglen = 0;
    if ((inend - inbuf) <= (int)sizeof(u_int32_t)) return;
    getword(inbuf, &msglen);
    if((msglen == 0) || (msglen > MAXMSGSIZ))
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "bad message size! (%d max)", MAXMSGSIZ);
        croakConnection(msg);
        return;
    }

    if (inend - inbuf >= (int)msglen + (int)sizeof(u_int32_t))
    {
        haveCompleteEvent = true;
    }
}

void
Client::croakConnection(const char *reason)
{
    if (!connected()) return;
    syslog(LOG_ERR, "libfam killing connection: %s", reason);
    close(sock);
    sock = -1;
    haveCompleteEvent = false;
}


static void
getword(const char *p, u_int32_t *l)
{
    memcpy(l, p, sizeof(u_int32_t));
    *l = ntohl(*l);
}

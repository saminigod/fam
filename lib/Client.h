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

#ifndef _client_
#define _client_
#include <sys/types.h>
#include "config.h"
#include "BTree.h"

#define MSGBUFSIZ 3000
#define MAXMSGSIZ 300

struct FAMEvent;

class Client {
    public:
	Client(long hostaddr, unsigned int prog, int vers);
	~Client();
	int writeToServer(char *msg, int nbytes);
        int getSock() { return sock; }
        bool connected() { return sock >= 0; }
        int eventPending();
        int nextEvent(FAMEvent *fe);

        void  storeUserData(int reqnum, void *p);
        void *getUserData(int reqnum);
        void  storeEndExist(int reqnum);
        bool  getEndExist(int reqnum);
        void  freeRequest(int reqnum);

    private:
        int readEvent(bool block);
        void checkBufferForEvent();
        void croakConnection(const char *reason);

	int sock;
        bool haveCompleteEvent;
        BTree<int, void *> *userData;
        BTree<int, bool>   *endExist;
	char *inend,inbuf[MSGBUFSIZ];
};
#endif

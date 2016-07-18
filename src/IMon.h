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

#ifndef IMon_included
#define IMon_included

#include "config.h"
#include <sys/stat.h>
#include <sys/types.h>

#include "Boolean.h"

struct stat;

//  IMon is an object encapsulating the interface to /dev/imon.
//  There can only be one instantiation of the IMon object.
//
//  The user of this object uses express() and revoke() to
//  express/revoke interest in a file to imon.  There is also
//  a callback, the EventHandler.  When an imon event comes in,
//  the EventHandler is called.
//
//  The user of the IMon object is the Interest class.

class IMon {

public:

    enum Status { OK = 0, BAD = -1 };
    enum Event { EXEC, EXIT, CHANGE };

    typedef void (*EventHandler)(dev_t, ino_t, int event);

    IMon(EventHandler h);
    ~IMon();

    static bool is_active();

    Status express(const char *name, struct stat *stat_return);
    Status revoke(const char *name, dev_t dev, ino_t ino);

private:

    //  Class Variables

    static int imonfd;
    static EventHandler ehandler;

    static void read_handler(int fd, void *closure);


    //
    // Low-level imon routines.
    //
    static int imon_open();
    Status imon_express(const char *name, struct stat *stat_return);
    Status imon_revoke(const char *name, dev_t dev, ino_t ino);

    IMon(const IMon&);			// Do not copy
    IMon & operator = (const IMon&);		//  or assign.

};

#endif /* !IMon_included */

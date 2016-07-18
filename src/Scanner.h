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

#ifndef Scanner_included
#define Scanner_included

#include "Boolean.h"
#include "Request.h"

class Client;
class Event;

//  Scanner is an abstract base class for a short-lived object that
//  scans an Interest.
//
//  Why?  Well, we can potentially generate a lot of data while
//  scanning a directory, so output could get blocked in mid scan, so
//  we need something to hold the scan's context while we wait for
//  output to unblock.
//
//  Since the output flow control is managed by the Client, the Client
//  owns the Scanner.  But the Scanner is created by a ClientInterest.
//
//  Yes, it's messy.

class Scanner {

public:

    virtual ~Scanner();
    virtual bool done();

};

#endif /* !Scanner_included */

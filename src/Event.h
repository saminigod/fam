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

#ifndef Event_included
#define Event_included

#include <fam.h>
#include <assert.h>


//  An Event is a FAM event (not an imon event or a select event).  An
//  Event has a type, which corresponds to the FAMCodes types defined
//  in <fam.h>.

class Event {

public:

    const char *name()    const;
    char code()           const;
    static const Event* getEventFromOpcode(char);
    int operator == (const Event& other) const  {return this == &other;}

    static const Event Changed;
    static const Event Deleted;
    static const Event Executing;
    static const Event Exited;
    static const Event Created;
//  Moved is not supported
//    static const Event Moved;
    static const Event Acknowledge;
    static const Event Exists;
    static const Event EndExist;

private: 
    static const Event Error;
    enum Type {
	ChangedT     = FAMChanged,        // 'c'
	DeletedT     = FAMDeleted,        // 'A'
	ExecutingT   = FAMStartExecuting, // 'X'
	ExitedT      = FAMStopExecuting,  // 'Q'
	CreatedT     = FAMCreated,        // 'F'
        MovedT       = FAMMoved,          // 'M'  (Not supported)
	AcknowledgeT = FAMAcknowledge,    // 'G'
	ExistsT      = FAMExists,         // 'e'
	EndExistT    = FAMEndExist,       // 'P'
        ErrorT                            // '?'
    };
    Event(Type n)                       : which(n){};

    const unsigned char which;

    // We don't want anyone using any Events that aren't the static
    // constant ones, so make it so Events can't be created or copied
    Event();                       // Don't use
    void operator =(const Event&); // Don't use


};

#endif /* !Event_included */

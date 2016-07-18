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

#include "Event.h"

#include "Log.h"

#include <assert.h>
#include <stdio.h>

const Event Event::Changed     = Event(ChangedT);
const Event Event::Deleted     = Event(DeletedT);
const Event Event::Executing   = Event(ExecutingT);
const Event Event::Exited      = Event(ExitedT);
const Event Event::Created     = Event(CreatedT);
const Event Event::Acknowledge = Event(AcknowledgeT);
const Event Event::Exists      = Event(ExistsT);
const Event Event::EndExist    = Event(EndExistT);
const Event Event::Error       = Event(ErrorT);


const Event*
Event::getEventFromOpcode(char opcode)
{
    switch (opcode)
    {
    case 'c':
        return &Changed;
    case 'A':
	return &Deleted;
    case 'X':
	return &Executing;
    case 'Q':
	return &Exited;
    case 'F':
	return &Created;
//    case 'M':  /* M is unsupported */
//        return Moved;
    case 'G':
	return &Acknowledge;
    case 'e':
        return &Exists;
    case 'P':
	return &EndExist;
    default:
	Log::error("unrecognized event opcode '%c' ('\0%o')",
		   opcode, opcode & 0377);
        return &Error;
    }
}

const char *
Event::name() const
{
    static char buf[40];

    switch (which)
    {
    case ChangedT:
	return "Changed";

    case DeletedT:
	return "Deleted";

    case ExecutingT:
	return "Executing";

    case ExitedT:
	return "Exited";

    case CreatedT:
	return "Created";

    case MovedT:
        return "Moved";
        
    case AcknowledgeT:
	return "Acknowledge";

    case ExistsT:
	return "Exists";

    case EndExistT:
	return "EndExist";

    default:
	sprintf(buf, "UNKNOWN EVENT %d", which);
	return buf;
    }
}

char
Event::code() const
{
    // Map event to letter.

    static const char codeletters[] = "?cAXQFMGeP";
    assert(which < sizeof codeletters - 1);
    char code = codeletters[which];
    assert(code != '?');
    return code;
}



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

#ifndef timeval_included
#define timeval_included

#include <sys/time.h>

//////////////////////////////////////////////////////////////////////////////
//  Define a few arithmetic ops on timevals.  Makes code much more
//  readable.
//
//  operators += and -= are defined in timeval.C.  The rest are
//  defined as inline functions.

timeval& operator += (timeval& left, const timeval& right);
timeval& operator -= (timeval& left, const timeval& right);

inline timeval
operator + (const timeval& left, const timeval& right)
{
    timeval result = left;
    result += right;
    return result;
}

inline timeval
operator - (const timeval& left, const timeval& right)
{
    timeval result = left;
    result -= right;
    return result;
}

inline int
operator < (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, < );
}

inline int
operator > (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, > );
}

inline int
operator >= (const timeval& left, const timeval& right)
{
    return !(left < right);
}

inline int
operator <= (const timeval& left, const timeval& right)
{
    return !(left > right);
}

inline int
operator == (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, == );
}

inline int
operator != (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, != );
}

#endif /* !timeval_included */

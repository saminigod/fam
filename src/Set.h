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

#ifndef Set_included
#define Set_included

#include "Boolean.h"
#include "BTree.h"

//  This implementation of a Set is sub-optimal!  It should
//  be reimplemented as a separate type.
//  Object of type "class T"  will will be cast to
//  unsigned int's internally, so they must have a conversion to
//  unsigned int and there should be a one-to-one mapping to unsigned
//  ints.
template <class T> class Set : public BTree<T, bool> {

public:

    typedef BTree<T, bool> inherited;

    Set()			       { }

    void insert(const T& e)	       { (void) inherited::insert(e, true); }
    bool contains(const T& e)       { return inherited::find(e); }
    // Inherit remove(), first(), next(), size(), sizeofnode() methods.

private:

    Set(const Set&);			// Do not copy
    Set & operator = (const Set&);		//  or assign.

};

#endif /* !Set_included */

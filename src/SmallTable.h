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

#ifndef SmallTable_included
#define SmallTable_included

#include <assert.h>

//  A SmallTable holds key-value mappings.  Its implementation is
//  cheap and dirty, so it should not be used for tables with lots of
//  entries.  Lookups are faster than insertions/removals.
//
// class TKey and Tvalue must define < and > operators, and must
//  be able to be assigned a value of 0 and be compared
//  with 0.
//
//  Operations:
//		insert()	puts a new entry in table.
//		remove()	removes an entry.
//		find()		returns the value for a key.
//		size()		returns number of entries.
//              first()         returns the first key


template <class Tkey, class Tvalue> class SmallTable {

public:

    SmallTable()			: n(0), nalloc(0), table(0) { }
    virtual ~SmallTable();

    void insert(const Tkey&, const Tvalue&);
    void remove(const Tkey&);
    void removeAll();
    inline Tvalue find(const Tkey& k) const;
    Tkey first() const			{ return n ? table[0].key : 0; }
    unsigned size() const		{ return n; }

private:

    struct Closure {
	const enum Presence { PRESENT, ABSENT } presence;
	const unsigned index;
	Closure(Presence p, unsigned i)	: presence(p), index(i) { }
    };

    struct Pair {
	Tkey key;
	Tvalue value;
    };

    unsigned n, nalloc;
    Pair *table;

    virtual Closure position(const Tkey&) const;

    SmallTable(const SmallTable&);	// Do not copy
    SmallTable & operator = (const SmallTable&);	//  or assign.

};


//
//  Template member implementations
//

template <class Tkey, class Tvalue>
inline Tvalue
SmallTable<Tkey, Tvalue>::find(const Tkey& k) const
{
    Closure c = position(k);
    return c.presence == Closure::PRESENT ? table[c.index].value : 0;
}

template <class Tkey, class Tvalue>
SmallTable<Tkey, Tvalue>::~SmallTable()
{
    delete [] table;
}

template <class Tkey, class Tvalue>
typename SmallTable<Tkey, Tvalue>::Closure
SmallTable<Tkey, Tvalue>::position(const Tkey& key) const
{
    unsigned l = 0, r = n;
    while (l < r)
    {
	unsigned mid = (l + r) / 2;
	if (key < table[mid].key)
	    r = mid;
	else if (key > table[mid].key)
	    l = mid + 1;
	else // (key == table[mid].key)
	    return Closure(Closure::PRESENT, mid);
    }
    return Closure(Closure::ABSENT, l);
}

template <class Tkey, class Tvalue>
void
SmallTable<Tkey, Tvalue>::insert(const Tkey& key, const Tvalue& value)
{
    Closure c = position(key);

    if (c.presence == Closure::PRESENT)
    {   table[c.index].value = value;
	return;
    }

    if (n >= nalloc)
    {
	//  Grow the table.

	nalloc = 3 * n / 2 + 10;
	Pair *nt = new Pair[nalloc];
	unsigned i;
	for (i = 0; i < c.index; i++)
	    nt[i] = table[i];
	for ( ; i < n; i++)
	    nt[i + 1] = table[i];
	delete [] table;
	table = nt;
    }
    else
    {
	// Shift following entries right.

	for (unsigned i = n; i > c.index; --i)
	    table[i] = table[i - 1];
    }
    table[c.index].key = key;
    table[c.index].value = value;
    n++;
}

template <class Tkey, class Tvalue>
void
SmallTable<Tkey, Tvalue>::remove(const Tkey& key)
{
    Closure c = position(key);
    assert(c.presence == Closure::PRESENT);

    // Shift following entries left.

    for (unsigned i = c.index + 1; i < n; i++)
	table[i - 1] = table[i];
    --n;

    // Shrink the table.

    if (2 * n < nalloc && n < nalloc - 10)
    {
	nalloc = n;
	Pair *nt = new Pair[nalloc];
	for (unsigned i = 0; i < n; i++)
	    nt[i] = table[i];
	delete [] table;
	table = nt;
    }
}

template <class Tkey, class Tvalue>
void
SmallTable<Tkey, Tvalue>::removeAll()
{
    n = 0;
    nalloc = 0;
    if (table != NULL) delete [] table;
    table = NULL;
}

#endif /* !SmallTable_included */

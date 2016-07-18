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

#ifndef StringTable_included
#define StringTable_included

#include <assert.h>
#include <string.h>

//  A StringTable maps C strings onto values.  It is a cheap O(n)
//  implementation, suitable only for small tables that are
//  infrequently searched.
//  class Tv must be able to be assigned a value of 0 and be compared
//  with 0.

template <class Tv> class StringTable {

    typedef const char *Tk;

public:

    StringTable()			: n(0), nalloc(0), table(0)
					{ }
    virtual ~StringTable();

    // Makes a copy of the Table.  Copys the keys, but not the values.
    StringTable& operator = (const StringTable&);

    inline Tv find(const Tk) const;
    unsigned size() const		{ return n; }
    Tk key(unsigned i) const      	{ return i < n ? table[i].key : 0; }
    Tv value(unsigned i) const		{ return i < n ? table[i].value : 0; }

    void insert(const Tk, const Tv&);
    void remove(const Tk);

private:

    struct Pair {
	char *key;
	Tv value;
    };

    unsigned n, nalloc;
    Pair *table;

    virtual signed int position(const Tk) const;

    StringTable(const StringTable&);	// Do not copy.

};


//
//  Template method implementations
//

template <class Tv>
inline Tv
StringTable<Tv>::find(const Tk k) const
{
    signed int index = position(k);
    return index >= 0 ? table[index].value : 0;
}

template <class Tv>
StringTable<Tv>::~StringTable()
{
    {
	for (unsigned i = 0; i < n; i++)
	    delete [] table[i].key;
    }
    delete[] table;

    table = NULL;
    nalloc = n = 0;
}

template <class Tv>
StringTable<Tv>&
StringTable<Tv>::operator = (const StringTable& that)
{
    if (this != &that)
    {
	unsigned i;
	for (i = 0; i < n; i++)
	    delete [] table[i].key;
	delete[] table;
	nalloc = n = that.n;
	table = new Pair[nalloc];
	for (i = 0; i < n; i++)
	{   const char *key = that.table[i].key;
	    table[i].key = strcpy(new char[strlen(key) + 1], key);
	    table[i].value = that.table[i].value;
	}
    }
    return *this;
}

template <class Tv>
signed int
StringTable<Tv>::position(const Tk key) const
{
    for (unsigned i = 0; i < n; i++)
	if (!strcmp(key, table[i].key))
	    return i;
    return -1;				// Not found.
}

template <class Tv>
void
StringTable<Tv>::insert(const Tk key, const Tv& value)
{
    signed int index = position(key);
    if (index >= 0)
    {   table[index].value = value;
	return;
    }

    if (n >= nalloc)
    {
	//  Grow the table.

	nalloc = 3 * n / 2 + 10;
	Pair *nt = new Pair[nalloc];
	for (unsigned i = 0; i < n; i++)
	    nt[i] = table[i];
	delete [] table;
	table = nt;
	assert(n < nalloc);
    }
    table[n].key = strcpy(new char[strlen(key) + 1], key);
    table[n].value = value;
    n++;
}

template <class Tv>
void
StringTable<Tv>::remove(const Tk key)
{
    signed int index = position(key);
    assert(index >= 0 && index < n);

    // Delete the matching key.

    delete [] table[index].key;
    table[index] = table[--n];

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

#endif /* !StringTable_included */

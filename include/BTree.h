//  Copyright (C) 1999-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef BTree_included
#define BTree_included

#include "Boolean.h"

//  This is an in-core B-Tree implementation.
//
//  The public interface is fairly sparse: find, insert and remove
//  are the provided functions.  insert() and remove() return true
//  on success, false on failure.  find() returns 0 on failure.
//
//  size() returns the number of key/value pairs in the tree.
//
//  sizeofnode() returns the size of a BTree node, in bytes.
//
//  BTree is instantiated by
//
//      libfam/Client.h:
//          BTree<int, void *> *userData;
//          BTree<int, bool>   *endExist;
//      fam/Listener.c++:
//          BTree<int, NegotiatingClient *> negotiating_clients;
//      fam/RequestMap.h:
//          typedef BTree<Request, ClientInterest *> RequestMap;
//      fam/Set.h:
//          template <class T> class Set : public BTree<T, bool>
//          typedef BTree<T, bool> inherited;
//
//  Set (derived from BTree) is instantiated by
//
//      fam/TCP_Client.h:
//          Set<Interest *> to_be_scanned;
//      fam/Pollster.h:
//          static Set<Interest *> polled_interests;
//          static Set<ServerHost *> polled_hosts;
//      fam/FileSystem.h:
//          typedef Set<ClientInterest *> Interests;
//
template <class Key, class Value> class BTree {

public:

    BTree();
    virtual ~BTree();

    Value find(const Key& k) const;
    bool insert(const Key& k, const Value& v);
    bool remove(const Key& k);

    Key first() const;
    Key next(const Key& k) const;

    unsigned size() const		{ return npairs; }

    static unsigned sizeofnode()	{ return sizeof (Node); }

private:

    enum { fanout = 32 };
    enum Status { OK, NO, OVER, UNDER };

    struct Node;
    struct Closure {

	Closure(Status s)		    : status(s), key(0),
                                              value(0), link(0) { }
	Closure(Key k, Value v, Node *l)    : status(OVER), key(k), value(v),
					      link(l) { }
	Closure(Status s, const Closure& i) : status(s), key(i.key),
					      value(i.value), link(i.link) { }
	Closure(Status s, const Key& k)	    : status(s), key(k),
                                              value(0), link(0) { }
	operator Status ()		    { return status; }

	Status status;
	Key    key;
	Value  value;
	Node  *link;

    };

    struct Node {

	Node(Node *, const Closure&);
	Node(Node *, unsigned index);
	~Node();

	unsigned find(const Key&) const;
	Closure next(const Key&) const;
	bool insert(unsigned, const Closure&);
	Closure remove(unsigned);
	void join(const Closure&, Node *);

	unsigned n;
	Key   key  [fanout];
	Node *link [fanout + 1];
	Value value[fanout];

    private:

	Node(const Node&);		// Do not copy
	Node& operator = (const Node&);	// or assign.

    };

    // Instance Variables

    Node *root;
    unsigned npairs;

    Closure insert(Node *, const Key&, const Value&);
    Status remove(Node *, const Key&);
    Status underflow(Node *, unsigned);
    Closure remove_rightmost(Node *);

    BTree(const BTree&);		// Do not copy
    BTree& operator = (const BTree&);	//  or assign.

};

//  Construct a Node from a Node and a Closure.  New node
//  has a single key-value pair, its left child
//  is the other node, and its right child is the link
//  field of the closure.

template <class K, class V>
BTree<K, V>::Node::Node(Node *p, const Closure& closure)
{
    n = 1;
    link[0] = p;
    key[0] = closure.key;
    value[0] = closure.value;
    link[1] = closure.link;
}

//  Construct a Node from a Node and an index.  Steals
//  the elements index..n of other Node for itself.
//  When this constructor is done, both this->link[0] and
//  that->link[that->n] point to the same place.  The caller
//  must resolve that.

template <class K, class V>
BTree<K, V>::Node::Node(Node *that, unsigned index)
{
    n = that->n - index;
    for (int i = 0; i < n; i++)
    {   key[i] = that->key[index + i];
	value[i] = that->value[index + i];
	link[i] = that->link[index + i];
    }
    link[n] = that->link[index + n];
    that->n = index;
}

//  Node destructor.  Recursively deletes subnodes.

template <class K, class V>
BTree<K, V>::Node::~Node()
{
    for (int i = 0; i <= n; i++)
	delete link[i];
}

//  Node::find() uses binary search to find the nearest key.
//  return index of key that matches or of next key to left.
//
//  E.g., if find() returns 3 then either key[3] matches or
//  key passed in is between key[3] and key[4].

template <class Key, class Value>
unsigned
BTree<Key, Value>::Node::find(const Key& k) const
{
    unsigned l = 0, r = n;
    while (l < r)
    {   int m = (l + r) / 2;
	if (k == key[m])
	    return m;
	else if (k < key[m])
	    r = m;
	else // (k > key[m])
	    l = m + 1;
    }
    assert(l == n || k < key[l]);
    return l;
}

//  Node::insert() inserts key/value into j'th position in node.
//  Inserts closure's link to the right.  Returns true if successful,
//  false if node is already full.

template <class K, class V>
bool
BTree<K, V>::Node::insert(unsigned j, const Closure& closure)
{
    if (n < fanout)
    {   for (int i = n; i > j; --i)
	{   key[i] = key[i - 1];
	    value[i] = value[i - 1];
	    link[i + 1] = link[i];
	}
	key[j] = closure.key; value[j] = closure.value;
	link[j + 1] = closure.link;
	n++;
	assert(j == 0     || key[j - 1] < key[j]);
	assert(j == n - 1 || key[j] < key[j + 1]);
	return true;
    }
    else
	return false;
}

//  Node::remove extracts the j'th key/value and the link
//  to the right and returns them.

template <class Key, class Value>
typename BTree<Key, Value>::Closure
BTree<Key, Value>::Node::remove(unsigned j)
{
    Key k = key[j];
    Value v = value[j];
    Node *l = link[j + 1];
    for (unsigned i = j + 1; i < n; i++)
    {   key[i - 1] = key[i];
	value[i - 1] = value[i];
	link[i] = link[i + 1];
    }
    --n;
    return Closure(k, v, l);
}

// Node::join() copies key/value from closure and moves all
// key/value/links from other Node into this node.
// Other Node is modified to contain no keys, no values, no links.

template <class K, class V>
void
BTree<K, V>::Node::join(const Closure& it, Node *that)
{
    assert(that);
    assert(n + that->n <= fanout - 1);
    key[n] = it.key;
    value[n] = it.value;
    for (int i = 0, j = n + 1; i < that->n; i++, j++)
    {   key[j] = that->key[i];
	value[j] = that->value[i];
	link[j] = that->link[i];
    }
    n += that->n + 1;
    link[n] = that->link[that->n];
    that->n = 0;
    that->link[0] = NULL;
}

///////////////////////////////////////////////////////////////////////////////

//  BTB constructor -- check that fanout is even.

template <class K, class V>
BTree<K, V>::BTree()
    : root(NULL), npairs(0)
{
    assert(!(fanout % 2));
}

//  BTB destructor -- delete all Nodes.

template <class K, class V>
BTree<K, V>::~BTree()
{
    delete root;
}

//  BTB::find() -- search BTB for Key, return matching value.

template <class Key, class Value>
Value
BTree<Key, Value>::find(const Key& key) const
{
    for (Node *p = root; p; )
    {   int i = p->find(key);
	if (i < p->n && key == p->key[i])
	    return p->value[i];
	else
	    p = p->link[i];
    }
    return Value(0);
}

template <class Key, class Value>
Key
BTree<Key, Value>::first() const
{
    if (!root)
	return 0;
    assert(root->n);

    Node *p, *q;
    for (p = root; q = p->link[0]; p = q)
	continue;
    return p->key[0];
}

template <class Key, class Value>
Key
BTree<Key, Value>::next(const Key& pred) const
{
    if (!root)
	return 0;

    assert(root->n);

    Closure it = root->next(pred);
    switch (Status(it))
    {
    case OK:
	return it.key;

    case OVER:
	return 0;			// All done.

    default:
	assert(0);
	return 0;
    }
}

template <class Key, class Value>
typename BTree<Key, Value>::Closure
BTree<Key, Value>::Node::next(const Key& pred) const
{
    if (!this)
	return OVER;

    unsigned i = find(pred);
    assert(i <= n);
    if (i < n && key[i] == pred)
	i++;
    Closure it = link[i]->next(pred);
    if (Status(it) != OVER)
	return it;
    if (i == n)
	return OVER;
    else
	return Closure(OK, key[i]);
}

//  BTB::insert() -- insert a new key/value pair
//  into the tree.  Fails and returns false if key is already
//  in tree.
//
//  This code is the only place that makes the tree taller.


template <class Key, class Value>
bool
BTree<Key, Value>::insert(const Key& key, const Value& value)
{
    Closure it = insert(root, key, value);
    switch (Status(it))
    {
    case OK:
	npairs++;
	return true;

    case NO:
	return false;

    case OVER:
	root = new Node(root, it);
	npairs++;
	return true;

    default:
	assert(0);
	return false;
    }
}

//  BTB::insert(Node *, ...) -- helper function for
//  BTB::insert(Key&, ...)  Recurses to the appropriate leaf, splits
//  nodes as necessary on the way back.

template <class Key, class Value>
typename BTree<Key, Value>::Closure
BTree<Key, Value>::insert(Node *p, const Key& key, const Value& value)
{
    if (!p) return Closure(key, value, NULL);
    //  If you're running Purify on a client linking with libfam, and it says
    //  that line is causing a 3-byte UMR for BTree<int, bool>::insert() in
    //  FAMNextEvent() ("Reading 8 bytes from 0x... on the stack (3 bytes at
    //  0x... uninit)"), and sizeof(bool) == 1, it's actually OK.  Those 3
    //  bytes are the padding between the bool Value and the Node *link in
    //  BTree<int, bool>::Closure.

    int i = p->find(key);
    if (i < p->n && key == p->key[i])
	return NO;			// key already exists.

    Closure it = insert(p->link[i], key, value);

    if (Status(it) == OVER)
    {   if (p->insert(i, it))
	    return Closure(OK);
	else
	{   if (i > fanout / 2)
	    {   Node *np = new Node(p, fanout / 2 + 1);
		np->insert(i - fanout / 2 - 1, it);
		assert(p->n > fanout / 2);
		it = p->remove(fanout / 2);
		return Closure(it.key, it.value, np);
	    }
	    else if (i < fanout / 2)
	    {
		Node *np = new Node(p, fanout / 2);
		p->insert(i, it);
		assert(p->n > fanout / 2);
		it = p->remove(fanout / 2);
		return Closure(it.key, it.value, np);
	    }
	    else // (i == fanout / 2)
	    {
		Node *np = new Node(p, fanout / 2);
		np->link[0] = it.link;
		return Closure(it.key, it.value, np);
	    }
	}
    }
    
    return it;
}

//  BTB::remove() -- remove a key/value from the tree.
//  This function is the only one that makes the tree shorter.

template <class Key, class Value>
bool
BTree<Key, Value>::remove(const Key& key)
{
    Status s = remove(root, key);
    switch (s)
    {
    case OK:
	assert(npairs);
	--npairs;
	assert(!root || root->n);
	return true;

    case NO:
	assert(!root || root->n);
	return false;

    case UNDER:
	if (root->n == 0)
	{   Node *nr = root->link[0];
	    root->link[0] = NULL;	// don't delete subtree
	    delete root;
	    root = nr;
	}
	assert(npairs);
	--npairs;
	assert(!root || root->n);
	return true;

    default:
	assert(0);
	return false;
    }
}

//  BTB::underflow -- helper function to BTB::remove() When a node
//  has too few elements (less than fanout / 2), this function is
//  invoked.  It tries to join i'th child of node p with one of its
//  adjacent siblings, then tries to move entries from an adjacent
//  sibling to child node.
//
//  Returns UNDER if node p is too small afterward, OK otherwise.

template <class Key, class Value>
typename BTree<Key, Value>::Status
BTree<Key, Value>::underflow(Node *p, unsigned i)
{
    assert(p);
    assert(i <= p->n);
    Node *cp = p->link[i];
    assert(cp);
    
    Node *rp = i < p->n ? p->link[i + 1] : NULL;
    Node *lp = i > 0    ? p->link[i - 1] : NULL;
    assert(!rp || rp->n >= fanout / 2);
    assert(!lp || lp->n >= fanout / 2);

    if (rp && rp->n == fanout / 2)
    {
	// join cp w/ rp;
	cp->join(p->remove(i), rp);
	delete rp;
    }
    else if (lp && lp->n == fanout / 2)
    {
	// join lp w/ cp;
	lp->join(p->remove(i - 1), cp);
	delete cp;
    }
    else if (lp)
    {
	// shuffle from lp to cp;
	Closure li = lp->remove(lp->n - 1);
	cp->insert(0, Closure(p->key[i - 1], p->value[i - 1], cp->link[0]));
	cp->link[0] = li.link;
	p->key[i - 1] = li.key;
	p->value[i - 1] = li.value;
	return OK;
    }
    else if (rp)
    {
	// shuffle from rp to cp;
	Closure ri = rp->remove(0);
	cp->insert(cp->n, Closure(p->key[i], p->value[i], rp->link[0]));
	p->key[i] = ri.key;
	p->value[i] = ri.value;
	rp->link[0] = ri.link;
	return OK;
    }
    
    return p->n >= fanout / 2 ? OK : UNDER;
}

//  BTB::remove_rightmost() -- helper to BTB::remove().
//
//  Removes rightmost leaf key/value from subtree and returns them.
//  If Nodes become too small in the process, invokes Node::underflow()
//  to fix them up.  Return status is either OK or UNDER, indicating
//  root of subtree is too small.


template <class Key, class Value>
typename BTree<Key, Value>::Closure
BTree<Key, Value>::remove_rightmost(Node *p)
{
    int i = p->n;
    Node *cp = p->link[i];
    if (cp)
    {   Closure it = remove_rightmost(cp);
	if (Status(it) == UNDER)
	    return Closure(underflow(p, i), it);
	else
	    return it;
    }
    else
    {   Closure it = p->remove(p->n - 1);
	if (p->n >= fanout / 2)
	    return Closure(OK, it);
	else
	    return Closure(UNDER, it);
    }	    
}

//  BTB::remove(Node *, ...) -- helper to BTB::remove(const Key&).
//
//  Recurses into tree to find key/value to delete.  When it finds
//  them, it pulls the rightmost leaf key/value off the branch to
//  the left and replaces the removed k/v with them.  Watches for
//  Node underflows from BTB::remove_rightmost() and propagates them
//  back up.

template <class Key, class Value>
typename BTree<Key, Value>::Status
BTree<Key, Value>::remove(Node *p, const Key& key)
{
    if (!p)
	return NO;			// key not found
    int i = p->find(key);
    if (i < p->n && key == p->key[i])
    {
	// Delete this one.

	Closure it = p->remove(i);
	if (p->link[i])
	{   Closure rm = remove_rightmost(p->link[i]);
	    assert(!rm.link);
	    p->insert(i, Closure(rm.key, rm.value, it.link));
	    if (Status(rm) == UNDER)
		return underflow(p, i);
	}
	return p->n >= fanout / 2 ? OK : UNDER;
    }
    else
    {   Node *cp = p->link[i];
	Status s = remove(cp, key);
	if (s == UNDER)
	    return underflow(p, i);
	else
	    return s;
    }
}



//#ifndef NDEBUG
//
//template <class K, class V>
//BTree<K, V>::BTree()
//{
//    //assert(sizeof K <= sizeof BTB::Key);
//    //assert(sizeof V <= sizeof BTB::Value);
//}
//
//#endif /* !NDEBUG */


#endif /* !BTree_included */

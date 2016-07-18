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

#ifndef Interest_included
#define Interest_included

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>  //  for in_addr

#include "Boolean.h"

class Event;
class FileSystem;
class IMon;
struct stat;

//  Interest -- abstract base class for filesystem entities of interest.
//
//  An Interest is monitored by imon or, if imon fails, it is polled
//  by the Pollster.
//
//  All Interests are kept in a global table keyed by dev/ino.
//
//  The classes derived from Interest are...
//
//	ClientInterest		an Interest a Client has explicitly monitored
//	Directory		a directory
//	File			a file
//	DirEntry		an entry in a monitored directory

class Interest {

public:

    enum ExportVerification { VERIFY_EXPORTED, NO_VERIFY_EXPORTED };

    Interest(const char *name, FileSystem *, in_addr host, ExportVerification);
    virtual ~Interest();

    const char *name() const		{ return myname; }
    bool exists() const		{ return old_stat.st_mode != 0; }
    bool isdir() const    { return (old_stat.st_mode & S_IFMT) == S_IFDIR; }
    virtual bool active() const = 0;
    bool needs_scan() const		{ return scan_state != OK; }
    void needs_scan(bool tf)		{ scan_state = tf ? NEEDS_SCAN : OK; }
    void mark_for_scan()		{ needs_scan(true); }
    virtual bool do_scan();
    void report_exec_state();

    virtual bool scan(Interest * = 0) = 0;
    virtual void unscan(Interest * = 0) = 0;
    void poll()				{ scan(); }

    //  Public Class Method

    static void imon_handler(dev_t, ino_t, int event);

    static void enable_xtab_verification(bool enable);

protected:

    bool do_stat();
    virtual void post_event(const Event&, const char * = NULL) = 0;
    char& ci_bits()			{ return ci_char; }
    char& dir_bits()			{ return dir_char; }
    const char& ci_bits() const		{ return ci_char; }
    const char& dir_bits() const	{ return dir_char; }
    const in_addr& host() const         { return myhost; }
    void verify_exported_to_host();
    bool exported_to_host() const       { return mypath_exported_to_host; }

private:

    enum { HASHSIZE = 257 };
    enum ScanState	{ OK, NEEDS_SCAN };
    enum ExecState	{ EXECUTING, NOT_EXECUTING };

    //  Instance Variables

    Interest *hashlink;
    dev_t dev;
    ino_t ino;
    char *const myname;
    ScanState     scan_state: 1;
    ExecState cur_exec_state: 1;
    ExecState old_exec_state: 1;
    char ci_char;
    char dir_char;
    struct stat old_stat;
    in_addr myhost;
    bool mypath_exported_to_host;
    

    //  Private Instance Methods

    bool dev_ino(dev_t, ino_t);
    void revoke();
    virtual void notify_created(Interest *) = 0;
    virtual void notify_deleted(Interest *) = 0;

    //  Class Variables

    static IMon imon;
    static Interest *hashtable[HASHSIZE];
    static bool xtab_verification;

    //  The Hashing Function

    static Interest **hashchain(dev_t d, ino_t i)
			  { return &hashtable[(unsigned) (d + i) % HASHSIZE]; }
    Interest **hashchain() const	{ return hashchain(dev, ino); }

    Interest(const Interest&);		// Do not copy
    Interest & operator = (const Interest&);	//  or assign.

};

#endif /* !Interest_included */

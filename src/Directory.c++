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

#include "Directory.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Client.h"
#include "DirEntry.h"
#include "DirectoryScanner.h"
#include "Event.h"
#include "FileSystem.h"
#include "Log.h"
#include "Scheduler.h"

Directory *Directory::current_dir;

Directory::Directory(const char *name, Client *c, Request r, const Cred& cr)
    : ClientInterest(name, c, r, cr, DIRECTORY), entries(NULL), unhangPid(-1)
{
    dir_bits() = 0;
    if (exported_to_host())
    {
        dir_bits() = SCANNING;
        DirectoryScanner *scanner = new DirectoryScanner(*this,
						         Event::Exists, false,
						         new_done_handler, this);
        if (scanner->done()) {
            delete scanner;
        } else {
	    client()->enqueue_scanner(scanner);
        }
    }
}

void
Directory::new_done_handler(void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~SCANNING;
    dir->post_event(Event::EndExist);
}

Directory::~Directory()
{
    assert(!(dir_bits() & SCANNING));
    if (dir_bits() & RESCAN_SCHEDULED)
	Scheduler::remove_onetime_task(scan_task, this);
    DirEntry *q, *p = entries;
    if (p)
    {   (void) chdir();
	while (p)
	{   q = p->next;
	    delete p;
	    p = q;
	}
    }
    if (current_dir == this)
	chdir_root();
}

ClientInterest::Type
Directory::type() const
{
    assert(!(dir_bits() & SCANNING));
    return DIRECTORY;
}

void
Directory::resume()
{
    assert(!(dir_bits() & SCANNING));
    ClientInterest::resume();
    for (DirEntry *ep = entries; ep; ep = ep->next)
	if (ep->needs_scan())
	    ep->scan();
}

Interest *
Directory::find_name(const char *name)
{
    assert(!(dir_bits() & SCANNING));
    if (name[0] == '/')
        // I don't know why this happens, but just in case ...
	return this;
    else
	for (DirEntry *ep = entries; ep; ep = ep->next)
	    if (!strcmp(name, ep->name()))
		return ep;
    return NULL;
}

//  Directory::do_scan() scans a Directory.  There are several cases.
//
//  If monitoring is suspended, do nothing.
//
//  If the Directory is actually not a directory (e.g., a file, a
//  device or nonexistent), then it is lstat'd once, and a Changed,
//  Deleted or Created event is written if appropriate.
//
//  If a Directory changes from a directory to something else, then
//  all its entries are deleted and Deleted events are sent.
//
//  If it is a real directory, then it is lstat'd repeatedly and read
//  repeatedly until it stops changing.  This prevents fam from
//  missing files in race conditions (I hope).

bool
Directory::do_scan()
{
    if (!active() || !needs_scan() || (dir_bits() & SCANNING))
	return false; 
    become_user();
    bool stat_changed = do_stat();
    if (stat_changed && !isdir())   // Seems like a bug to send Changed after
	post_event(Event::Changed); // Deleted, but what would fixing it break?
    bool scan_entries = filesystem()->dir_entries_scanned();
    dir_bits() |= SCANNING;
    DirectoryScanner *scanner = new DirectoryScanner(*this, Event::Created,
						     scan_entries,
						     scan_done_handler,
						     this);
    if (scanner->done()) {
        delete scanner;
    } else {
	client()->enqueue_scanner(scanner);
    }
    return stat_changed;
}

void
Directory::scan_task(void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~RESCAN_SCHEDULED;
    dir->scan();
}

void
Directory::scan_done_handler(void *closure)
{
    Directory *dir = (Directory *) closure;
    dir->dir_bits() &= ~SCANNING;
}

bool
Directory::chdir()
{
    if (current_dir == this)
	return true;

    int rc = ::chdir(name());
    Log::debug("+chdir to \"%s\"", name());
    if (rc < 0)
    {   Log::info("can't chdir(\"%s\"): %m", name());
	if ((errno == EACCES) && (client() != NULL))
        {
            client()->suggest_insecure_compat(name());
        }
	return false;
    }

    current_dir = this;
    return true;
}

bool
Directory::chdir_root()
{
    int rc = 0;
    if (current_dir)
    {
        rc = ::chdir("/");
        Log::debug("-chdir to \"/\"");
	current_dir = NULL;
    }
    return rc == 0;
}

//
//  void
//  Directory::unhang()
//
//  Description:
//      This gets called after a system call has failed with oserror()
//      set to ETIMEDOUT.  This means that we can't contact the nfs
//      server.  In order to reconnect when the server becomes visible
//      again, a process has to acually hang on the mount.  We fork and
//      exec nfsunhang to do this for us.  We don't care about
//      nfsunhang's exit status because we poll nfs mounts anyway.
//
#if HAVE_SGI_NOHANG
void
Directory::unhang()
{
    int status;
    if (unhangPid == -1
	|| waitpid(unhangPid, &status, WNOHANG) != 0) {

	if (access("/usr/lib/nfsunhang", X_OK) == -1) {
	    return;
	}

	unhangPid = fork();
	if (unhangPid == 0) {
	    for (int fd = getdtablesize() - 1; fd > 2; fd--) {
		close(fd);
	    }
	    execl("/usr/lib/nfsunhang", "nfsunhang", name(), NULL);
	    exit(1);
	}
	Log::debug("unhangPid: %d", unhangPid);
    }
}
#endif // HAVE_SGI_NOHANG

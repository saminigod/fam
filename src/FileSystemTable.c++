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

#include <stddef.h>
#include "FileSystemTable.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STATVFS
#include <sys/statvfs.h>
#endif

#include "Cred.h"
#include "Event.h"
#include "FileSystem.h"
#include "InternalClient.h"
#include "LocalFileSystem.h"
#include "Log.h"
#include "NFSFileSystem.h"

//  Fam has two tables of mounted filesystems -- fs_by_name and
//  fs_by_id.  They are keyed by mountpoint and by filesystem ID,
//  respectively.  fs_by_id is lazily filled in as needed (so we only
//  do as many statvfs calls as needed -- fam may hang if the NFS
//  server is down).  fs_by_name is completely rebuilt when /etc/mtab
//  is changed, and fs_by_id is destroyed, to be lazily re-filled
//  later.

//  Class Variables

unsigned int		    FileSystemTable::count;
FileSystemTable::IDTable    FileSystemTable::fs_by_id;
FileSystemTable::NameTable *FileSystemTable::fs_by_name;
const char		    FileSystemTable::mtab_name[] = MOUNTED;
InternalClient		   *FileSystemTable::mtab_watcher;
FileSystem		   *FileSystemTable::root;

#ifdef HAPPY_PURIFY

//////////////////////////////////////////////////////////////////////////////
//  The constructor and destructor simply maintain a refcount of the
//  files that include FileSystemTable.h.  When the last reference
//  is destroyed, the mtab watcher is turned off, and the fs_by_name
//  table is destroyed.

FileSystemTable::FileSystemTable()
{
    count++;
}

FileSystemTable::~FileSystemTable()
{
    if (!--count)
    {   delete mtab_watcher;
	mtab_watcher = NULL;
	if (fs_by_name)
	{   destroy_fses(fs_by_name);
	    delete fs_by_name;
	    fs_by_name = NULL;
	}
    }
}

#endif /* HAPPY_PURIFY */

//////////////////////////////////////////////////////////////////////////////
//  fs_by_name is a table that maps mount directories to FileSystem pointers.
//
//  It is built the first time FileSystemTable::find() is called, and
//  it's rebuilt whenever /etc/mtab changes.  When it is rebuilt,
//  existing FileSystems are moved to the new table.  This is done
//  because each ClientInterest has a pointer to its FileSystem,
//  and we don't want to change all ClientInterests, nor do we want
//  two FileSystem structures representing the same file system.


void
FileSystemTable::create_fs_by_name()
{
    NameTable *new_fs_by_name = new NameTable;
    NameTable mount_parents, dismounted_fses;

    if (fs_by_name)
	dismounted_fses = *fs_by_name;

    //  Read /etc/mtab.
    Cred::SuperUser.become_user();
    FILE *mtab = setmntent(mtab_name, "r");
    if(mtab == NULL)
    {
        Log::error("couldn't open %s for reading", mtab_name);
        delete new_fs_by_name;
        return;
    }
    root = NULL;
    for (mntent *mp; ((mp = getmntent(mtab)) != NULL); )
    {
	FileSystem *fs = fs_by_name ? fs_by_name->find(mp->mnt_dir) : NULL;
	if (fs && fs->matches(*mp))
	{
	    Log::debug("mtab: MATCH     \"%s\" on \"%s\" using type <%s>",
		       mp->mnt_fsname, mp->mnt_dir, mp->mnt_type);

	    new_fs_by_name->insert(mp->mnt_dir, fs);
	    if (dismounted_fses.find(mp->mnt_dir))
		dismounted_fses.remove(mp->mnt_dir);
	}
	else
	{

            if ((!strcmp(mp->mnt_type, MNTTYPE_NFS)
#if HAVE_MNTTYPE_NFS2
                || !strcmp(mp->mnt_type, MNTTYPE_NFS2)
#endif
#if HAVE_MNTTYPE_NFS3
                || !strcmp(mp->mnt_type, MNTTYPE_NFS3)
#endif
#if HAVE_MNTTYPE_CACHEFS
                || !strcmp(mp->mnt_type, MNTTYPE_CACHEFS)
#endif
                ) && strchr(mp->mnt_fsname, ':'))
            {
                if(Log::get_level() == Log::DEBUG)
                {
                    const char *mntopt = hasmntopt(mp, "dev");
                    if(mntopt == NULL) mntopt = "";
		    Log::debug("mtab: new NFS   \"%s\" on \"%s\" %s using <%s>",
			       mp->mnt_fsname, mp->mnt_dir, mntopt,
                               mp->mnt_type);
                }

		fs = new NFSFileSystem(*mp);
	    }
	    else
	    {
		Log::debug("mtab: new local \"%s\" on \"%s\"",
			   mp->mnt_fsname, mp->mnt_dir);

		fs = new LocalFileSystem(*mp);
	    }
	    new_fs_by_name->insert(mp->mnt_dir, fs);
	    if (fs_by_name)
	    {
		// Find parent filesystem.

		FileSystem *parent = longest_prefix(mp->mnt_dir);
		assert(parent);
		mount_parents.insert(parent->dir(), parent);
	    }
	}
	if (!strcmp(mp->mnt_dir, "/"))
	    root = fs;
    }
    endmntent(mtab);

    if(root == NULL)
    {
        assert(root);
        Log::error("couldn't find / in %s", mtab_name);
        delete new_fs_by_name;
        return;  //  horrible... we're not in a good state, the
                 //  now-brain-damaged fs_by_name is hanging around, etc.
                 //  It might be better to exit.
    }

    //  Install the new table.

    delete fs_by_name;
    fs_by_name = new_fs_by_name;

    //  Relocate all interests in parents of new filesystems.
    //  We relocate interests out of parents before relocating
    //  out of dismounted filesystems in the hope that we can
    //	avoid relocating some interests twice.  Consider the case
    //  where /mnt/foo is an interest, and we simultaneously
    //  learn that /mnt was dismounted and /fred was mounted.
    //  We don't want to relocate /mnt/foo to /, then test
    //  it for relocation to /fred.

    unsigned i;
    FileSystem *fs;
    for (i = 0; ((fs = mount_parents.value(i)) != NULL); i++)
    {
	Log::debug("mtab: relocating in parent \"%s\"", fs->dir());
	fs->relocate_interests();
    }

    //  Relocate all interests in dismounted filesystems and destroy
    //  the filesystems.

    for (i = 0; ((fs = dismounted_fses.value(i)) != NULL); i++)
    {
	Log::debug("mtab: dismount  \"%s\" on \"%s\"",
		   fs->fsname(), fs->dir());

	fs->relocate_interests();
	delete fs;
    }
    Log::debug("mtab done.");
}

void
FileSystemTable::destroy_fses(NameTable *fstab)
{
    //  Destroy any unreclaimed filesystems.

    for (unsigned i = 0; fstab->key(i); i++)
	delete fstab->value(i);
}

void
FileSystemTable::mtab_event_handler(const Event& event, void *)
{
    if (event == Event::Changed)
    {
	Log::debug("%s changed, rebuilding filesystem table", mtab_name);
	fs_by_id.removeAll();
	create_fs_by_name();
    }
}

//////////////////////////////////////////////////////////////////////////////
//

FileSystem *
FileSystemTable::find(const char *path, const Cred& cr)
{
    char temp_path[PATH_MAX];
    FileSystem *fs = NULL;

    assert(path[0] == '/');

    //  (Initialize fs_by_name if necessary.) As a side effect,
    //  create_fs_by_name initializes our "root" member variable.
    if (!fs_by_name)
    {   create_fs_by_name();
	mtab_watcher = new InternalClient(mtab_name, mtab_event_handler, NULL);
    }

    cr.become_user();
    
    //  If !HAVE_STATVFS, we could use statfs instead, but the statfs.f_fsid
    //  is not set reliably on Linux, so it's useless.  We'll do every lookup
    //  by name; hopefully that doesn't suck.
#if HAVE_STATVFS

    //  Perform a statvfs(2) on the first existing ancestor.

    struct statvfs fs_status;
    int rc = statvfs(path, &fs_status);
    if (rc < 0)
    {   (void) strcpy(temp_path, path);
	while (rc < 0)
	{   char *slash = strrchr(temp_path, '/');
	    if (!slash)
		return root;
	    *slash = '\0';
	    rc = statvfs(temp_path, &fs_status);
	}
    }

    //  Look up filesystem by ID.

    fs = fs_by_id.find(fs_status.f_fsid);
    if (fs)
	return fs;
#endif

    //  Convert to real path.  Look up real path in fs_by_name().

    cr.become_user();
    (void) realpath(path, temp_path);
    fs = longest_prefix(temp_path);

    //  Insert into fs_by_id.

#if HAVE_STATVFS
    fs_by_id.insert(fs_status.f_fsid, fs);
#endif

    return fs;
}

FileSystem *
FileSystemTable::longest_prefix(const char *path)
{
    FileSystem * bestmatch = root;
    int maxmatch = -1;
    const char *key;
    for (unsigned i = 0; ((key = fs_by_name->key(i)) != NULL); i++)
    {   for (int j = 0; ; j++)
	{   if (!key[j])
	    {   if ((!path[j] || path[j] == '/') && j > maxmatch)
		{   maxmatch = j;
		    bestmatch = fs_by_name->value(i);
		}
		break;
	    }
	    if (key[j] != path[j])
		break;
	}
    }
    assert(bestmatch);
    return bestmatch;
}


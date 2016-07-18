//  Copyright (C) 1999-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#if HAVE_SYSSGI
#include <sys/syssgi.h>
#endif
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "Activity.h"
#include "Listener.h"
#include "Log.h"
#include "Pollster.h"
#include "Scheduler.h"
#include "Cred.h"
#include "Interest.h"

const char *program_name;

//  These are options which are read from the configuration file & the
//  command line.
struct config_opts
{
    config_opts();
    const char *config_file;  // do not free this
    char *untrusted_user;
    int pollster_interval;  // in seconds
    int activity_timeout;   // in seconds
    bool disable_pollster;
    bool local_only;
    bool xtab_verification;
    bool disable_audit;
    bool disable_mac;
    bool insecure_compat;
    bool debugging;
};
#define CFG_INSECURE_COMPAT "insecure_compatibility"
#define CFG_LOCAL_ONLY "local_only"
#define CFG_XTAB_VERIFICATION "xtab_verification"
#define CFG_UNTRUSTED_USER "untrusted_user"
#define CFG_IDLE_TIMEOUT "idle_timeout"
#define CFG_NFS_POLLING_INTERVAL "nfs_polling_interval"
static void parse_config(config_opts &opts);
static void parse_config_line(config_opts &opts, int line,
                              const char *k, const char *v);
static bool is_true(const char *str);

static const char *basename2(const char *p)
{
    const char *tail = p;
    while (*p)
	if (*p++ == '/' && *p && *p != '/')
	    tail = p;
    return tail;
}

void usage()
{   fprintf(stderr,
	    "fam, version %s\n"
	    "Use: %s [ -f | -d | -v ] [ -l | -t seconds ] [ -T seconds ] \\\n"
	    "\t\t\t\t[ -p prog.vers ] [ -L ] [ -c config_file ] [-C]\n",
	    VERSION, program_name);
    fprintf(stderr, "\t-f\t\tstay in foreground\n");
    fprintf(stderr, "\t-d\t\tdebug\n");
    fprintf(stderr, "\t-v\t\tverbose\n");
    fprintf(stderr, "\t-l\t\tno polling\n");
    fprintf(stderr, "\t-t seconds\tset polling interval (default 6 s)\n");
    fprintf(stderr, "\t-T seconds\tset inactive timeout (default 5 s)\n");
    fprintf(stderr, "\t-p prog.vers\tset RPC program number and version\n");
    fprintf(stderr, "\t-L\t\tlocal only (ignore remote requests)\n");
    fprintf(stderr, "\t-c config_file\tpath to alternate configuration file\n");
    fprintf(stderr, "\t\t\t  (default is %s)\n", FAM_CONF);
    fprintf(stderr, "\t-C\t\tinsecure compatibility\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main(int argc, char *argv[])
{
#if HAVE_SGI_NOHANG
    // This keeps down nfs mounts from hanging fam.  System calls on
    // down nfs mounts fail with errno == ETIMEDOUT.  We don't do this
    // if we're running diskless because we don't want fam to crash
    // because a page fault timed out.
    char buf[20];
    if (sgikopt("diskless", buf, sizeof buf) == 0
	&& strcmp(buf, "0") == 0) {
	syssgi(SGI_NOHANG, 1);
    }
#endif

    config_opts opts;

    //  If fd 0 is a socket, we figure we were started by inetd.
    struct stat st;
    fstat(0, &st);
    bool started_by_inetd = S_ISSOCK(st.st_mode);

    unsigned long program = Listener::FAMPROG, version = Listener::FAMVERS;

    program_name = basename2(argv[0]);
    Log::name(program_name);

    if (!started_by_inetd)
	Log::foreground();

    //  Run through the command-line args once, looking for debugging flags
    //  and config-file flags.  (We want to check those before we parse the
    //  config file, and we want to parse the config file before reading other
    //  command-line args which might override its contents.)
    int i;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-' || !argv[i][1] || argv[i][2]) continue;
        switch (argv[i][1])
        {
        case 'c':
            if(++i >= argc) usage();
            opts.config_file = argv[i];
            break;
	case 'f':
	    opts.debugging = true;
	    break;
	case 'd':
	    Log::debug();
	    opts.debugging = true;
	    break;
	case 'v':
	    Log::info();
	    opts.debugging = true;
	    break;
        }
    }

    if (getuid() != 0)
    {   Log::error("must be superuser");
	exit(1);
    }

    parse_config(opts);

    //  Now run through the command-line arguments again.
    for (i = 1; i < argc; i++)
    {   if (argv[i][0] != '-' || !argv[i][1] || argv[i][2])
	    usage();
	switch (argv[i][1])
	{
	    char *p, *q;
	    unsigned secs;

	case 'f':
	case 'd':
	case 'v':
            //  handled above.
	    break;

	case 'l':
	    opts.disable_pollster = true;
	    break;

	case 'p':
	    if (++i >= argc)
		usage();
	    p = strchr(argv[i], '.');
	    if (p)
	    {	*p++ = '\0';
		version = strtoul(p, &q, 10);
                if (p == q) usage();
	    }
	    program = strtoul(argv[i], &q, 10);
            if (argv[i] == q) usage();
	    break;

	case 't':
	    if (i + 1 >= argc)
		usage();
	    secs = strtoul(argv[++i], &p, 10);
	    if (*p)
		usage();
	    if (secs == 0)
		Log::error("illegal poll interval 0");
	    else
		opts.pollster_interval = secs;
	    break;

	case 'T':
	    if (i + 1 >= argc)
		usage();
	    secs = strtoul(argv[++i], &p, 10);
	    if (*p)
		usage();
	    opts.activity_timeout = secs;
	    break;

	case 'L':
	    opts.local_only = true;
	    break;

	case 'C':
	    opts.insecure_compat = true;
            opts.xtab_verification = false;
            Log::info("Running with -C (" CFG_INSECURE_COMPAT
                      ") command-line option");
	    break;

        case 'c':
            //  handled above.
            ++i;
            break;

	default:
	    usage();
	}
    }

    //  Apply the various options.
    if (opts.local_only && started_by_inetd) {
        Log::error("Warning!  Started by inetd, so -L (" CFG_LOCAL_ONLY
                   ") option is being ignored!");
        opts.local_only = false;
    }
    if (opts.disable_audit) Log::disable_audit();
    if (opts.disable_mac) Cred::disable_mac();
    if (opts.insecure_compat) Cred::enable_insecure_compat();
    if (opts.untrusted_user) Cred::set_untrusted_user(opts.untrusted_user);
    else {
        Log::error("Fatal misconfiguration!  No " CFG_UNTRUSTED_USER
                   " found in %s!", opts.config_file);
        exit(1);
    }
    Pollster::interval(opts.pollster_interval);
    Activity::timeout(opts.activity_timeout);
    if (opts.disable_pollster) Pollster::disable();
    if (!opts.local_only) {
        Interest::enable_xtab_verification(opts.xtab_verification);
    }
    Log::audit(true, "fam starting SAT with process name \"%s\"",
                     program_name);

    if (!started_by_inetd)
    {
#if HAVE_DAEMONIZE
	if (!opts.debugging)
	{   _daemonize(0, -1, -1, -1);
	    Log::background();
	}
#else
#  if HAVE_DAEMON
	if (!opts.debugging)
	{   daemon(0, 0);
	    Log::background();
	}
#  endif
#endif
    }
    (void) signal(SIGPIPE, SIG_IGN);

#if HAVE_SGI_NOHANG
    // Ignore SIGCHLD because we run nfsunhang to unhang down nfs
    // mounts, and we don't care about the exit status of nfsunhang
    // (since we poll anyway) and we don't want to create zombies.
    (void) signal(SIGCHLD, SIG_IGN);
#endif
    new Listener(started_by_inetd, opts.local_only, program, version);
    Scheduler::loop();
    return 0;
}


static void
parse_config(config_opts &opts)
{
    FILE *cfg = fopen(opts.config_file, "r");
    if(cfg == NULL)
    {
        Log::error("Couldn't open config file \"%s\"!", opts.config_file);
        usage();
        return;
    }
    char buf[PATH_MAX + 64];
    char *bp;
    int lineno = 0;
    while(fgets(buf, sizeof(buf), cfg))
    {
        ++lineno;
        bp = buf;
        while(isspace(*bp)) ++bp;
        if ((*bp == '\0') || (*bp == '#') || (*bp == '!')) continue;
        if (bp[strlen(bp) - 1] != '\n')
        {
            Log::error("config file %s line %d is too long, and is "
                       "being ignored!", opts.config_file, lineno);
            --lineno;
            continue;
        }
        bp[strlen(bp) - 1] = '\0';  // whap newline

        //  split line at "=", strip whitespace from ends of both pieces
        char *vp = strchr(bp, '=');
        if(vp == NULL)
        {
            Log::error("config file %s line %d seems dain bramaged (no \"=\"), "
                       "and is being ignored.", opts.config_file, lineno);
            continue;
        }
        *vp++ = '\0';
        while (isspace(*vp)) ++vp;
        char *end = bp + strlen(bp) - 1;
        while (isspace(*end) && (end >= bp)) *end-- = '\0';
        end = vp + strlen(vp) - 1;
        while (isspace(*end) && (end >= vp)) *end-- = '\0';

        Log::debug("read %s line %d: \"%s\" = \"%s\"",
                   opts.config_file, lineno, bp, vp);
        parse_config_line(opts, lineno, bp, vp);
    }
    fclose(cfg);
}



static void
parse_config_line(config_opts &opts, int lineno, const char *key, const char *val)
{
    char *p;
    unsigned secs;

    if(!strcmp(key, CFG_UNTRUSTED_USER))
    {
        if (!opts.untrusted_user) opts.untrusted_user = strdup(val);
        else Log::error("config file %s line %d: ignoring duplicate %s",
                        opts.config_file, lineno, key);
    }
    else if(!strcmp(key, CFG_LOCAL_ONLY))
    {
        opts.local_only = is_true(val);
    }
    else if(!strcmp(key, CFG_IDLE_TIMEOUT))
    {
	secs = strtoul(val, &p, 10);
	if (*p)
	{
	    Log::error("config file %s line %d: ignoring invalid value for %s",
	    		opts.config_file, lineno, key);
	}
	else
	{
	    opts.activity_timeout = secs;
	}
    }
    else if(!strcmp(key, CFG_NFS_POLLING_INTERVAL))
    {
	secs = strtoul(val, &p, 10);
	if (*p || secs == 0)
	{
	    Log::error("config file %s line %d: ignoring invalid value for %s",
		       opts.config_file, lineno, key);
	}
	else
	{
	    opts.pollster_interval = secs;
	}
    }
    else if(!strcmp(key, CFG_XTAB_VERIFICATION))
    {
        opts.xtab_verification = is_true(val);
        if(opts.xtab_verification && opts.insecure_compat)
        {
            opts.xtab_verification = false;
            Log::error("config file %s line %d: ignoring %s because "
                       CFG_INSECURE_COMPAT " is set",
                       opts.config_file, lineno, key);
        }
    }
    else if(!strcmp(key, CFG_INSECURE_COMPAT))
    {
        opts.insecure_compat = is_true(val);
//ehh... it would be nice to handle this a little better.
//        if(opts.insecure_compat && opts.xtab_verification)
//        {
//            opts.xtab_verification = false;
//            Log::error("config file %s line %d: %s overrides "
//                       CFG_XTAB_VERIFICATION,
//                       opts.config_file, lineno, key);
//        }
    }
    else if(!strcmp(key, "disable_audit"))
    {
        opts.disable_audit = is_true(val);
    }
    else if(!strcmp(key, "disable_mac"))
    {
        opts.disable_mac = is_true(val);
    }
    else
    {
        Log::error("config file %s line %d: unrecognized key \"%s\"",
                    opts.config_file, lineno, key);
    }
}


config_opts::config_opts()
{
    memset(this, 0, sizeof(config_opts));

    config_file = FAM_CONF;
    untrusted_user = NULL;
    pollster_interval = 6;
    activity_timeout = 5;
    disable_pollster = false;
    local_only = false;
    xtab_verification = true;
#ifdef HAVE_AUDIT
    disable_audit = (sysconf(_SC_AUDIT) != 1);
#else
    disable_audit = true;
#endif
#ifdef HAVE_MAC
    disable_mac = ((sysconf(_SC_MAC) != 1) || (sysconf(_SC_IP_SECOPTS) != 1));
#else
    disable_mac = true;
#endif
    insecure_compat = false;
    debugging = false;
}

static bool
is_true(const char *val)
{
    if (!strcasecmp(val, "false") ||
        !strcasecmp(val, "no")  ||
        !strcmp(val, "0"))
    {
        return false;
    }
    return true;
}

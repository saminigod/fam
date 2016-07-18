//  Copyright (C) 1999-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "Log.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_AUDIT
#include <sat.h>
#endif

Log::LogLevel Log::level         = ERROR;
bool       Log::log_to_stderr = false;
const char   *Log::program_name  = "fam";
bool       Log::syslog_open   = false;
unsigned      Log::count         = 0;
#ifdef HAVE_AUDIT
bool       Log::audit_enabled = true;
#endif

void
Log::debug()
{
    level = DEBUG;
    info("log level is LOG_DEBUG");
}

void
Log::info()
{
    level = INFO;
    info("log level is LOG_INFO");
}

void
Log::error()
{
    level = INFO;
    info("log level is LOG_ERR");
    level = ERROR;
}

void
Log::disable_audit()
{
#ifdef HAVE_AUDIT
    audit_enabled = false;
    info("disabling security audit trail");
#endif
}

void
Log::foreground()
{
    log_to_stderr = true;
    if (syslog_open)
    {   closelog();
	syslog_open = false;
    }
}

void
Log::background()
{
    log_to_stderr = false;
}

void
Log::name(const char *newname)
{
    program_name = newname;
    audit(true, "fam changing process name to \"%s\"", newname);
}

void
Log::debug(const char *fmt, ...)
{
    if (level >= DEBUG)
    {
	va_list a;
	va_start(a, fmt);
	vlog(DEBUG, fmt, a);
	va_end(a);
    }
}

void
Log::info(const char *fmt, ...)
{
    if (level >= INFO)
    {
	va_list a;
	va_start(a, fmt);
	vlog(INFO, fmt, a);
	va_end(a);
    }
}

void
Log::error(const char *fmt, ...)
{
    if (level >= ERROR)
    {
	va_list a;
	va_start(a, fmt);
	vlog(ERROR, fmt, a);
	va_end(a);
    }
}

void
Log::critical(const char *fmt, ...)
{
    if (level >= CRITICAL)
    {
	va_list a;
	va_start(a, fmt);
	vlog(CRITICAL, fmt, a);
	va_end(a);
    }
}

void
Log::perror(const char *format, ...)
{
    if (level >= ERROR)
    {   char * buf = new char[strlen(format) + 5];
	(void) strcpy(buf, format);
	(void) strcat(buf, ": %m");
	va_list args;
	va_start(args, format);
	vlog(ERROR, buf, args);
	va_end(args);
        delete[] buf;
    }
}

void
Log::audit(bool success, const char *format, ...)
{
#ifdef HAVE_AUDIT
    if (audit_enabled)
    {
        //  This is not so good.  If there were a version of satwrite
        //  which took a va_list, this buffer would not need to be here.
        char buf[SAT_MAX_USER_REC];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, SAT_MAX_USER_REC, format, args);
        va_end(args);
        if(len > 0) satwrite(SAT_AE_CUSTOM, success ? SAT_SUCCESS : SAT_FAILURE,
                             buf, len);
    }
#endif
}

//  This is like debug(), info(), & error(), but it logs regardless of the
//  current log level.
void
Log::log(LogLevel l, const char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    vlog(l, fmt, a);
    va_end(a);
}

void
Log::vlog(LogLevel l, const char *format, va_list args)
{
    if (log_to_stderr)
        vfglog(format, args);
    else {
        if (!syslog_open) {
            openlog(program_name, LOG_PID, LOG_DAEMON);
            syslog_open = true;
        }
        int ll;
        switch (l) {
        case DEBUG:
            ll = LOG_DEBUG; break;
        case INFO:
            ll = LOG_INFO;  break;
        case ERROR:
            ll = LOG_ERR;   break;
        case CRITICAL:
        default:
            ll = LOG_CRIT;   break;
        }
        vsyslog(ll, format, args);
    }
}

void
Log::vfglog(const char *format, va_list args)
{
    // Count number of %'s in the format string.  That's the max
    // number of %m's that there can be.
    int numPercents = 0;
    const char *pt = format;
    while(*pt) {
        if (*pt++ == '%') numPercents++;
    }
    char * err;

    // Only get the error string if there's a chance we'll use it.
    if (numPercents > 0) {
        err =  strerror(errno);
        if (err == NULL) {
            err = "Unknown error";
        }
    } else {
        err = "";
    }

    //  This 2 is for the \n and the null terminator added by the strcpy.
    char *buf = new char[strlen(format) + 2 + strlen(err) *
                        numPercents];
    char *p = buf;
    while (*format) {
        if (format[0] == '%' && format[1] == 'm') {
            p += strlen(strcpy(p, err));
            format += 2;
        } else {
            *p++ = *format++;
        }
    }
    (void) strcpy(p, "\n");
    (void) fprintf(stderr, "%s[%d]: ", program_name, getpid());
    (void) vfprintf(stderr, buf, args);
    delete[] buf;
}

#ifndef NDEBUG

//  New back end for assert() will log to syslog, put core file
//  in known directory.

void __assert(const char *msg, const char *file, int line)
{
    char *dirname = new char[strlen(Log::getName()) + 20];
    (void) sprintf(dirname, "/usr/tmp/%s.%d", Log::getName(), getpid());
    Log::error("Assertion failed at %s line %d: %s", file, line, msg);
    Log::error("Dumping core in %s/core", dirname);

    if (setreuid(0, 0) < 0)
	Log::perror("setreuid");
    if (mkdir(dirname, 0755) < 0)
	Log::perror("mkdir");
    if (chdir(dirname) < 0)
	Log::perror("chdir");
    struct rlimit rl = { RLIM_INFINITY, RLIM_INFINITY };
    if (setrlimit(RLIMIT_CORE, &rl) < 0)
	Log::perror("setrlimit(RLIMIT_CORE)");
    delete[] dirname;
    abort();
}

#endif /* !NDEBUG */

#ifdef HAPPY_PURIFY

Log::Log()
{
    count++;
}

Log::~Log()
{
    if (!--count)
    {   if (syslog_open)
	{   closelog();
	    syslog_open = false;
	}
    }
}

#endif /* HAPPY_PURIFY */

#ifdef UNIT_TEST_Log

#include <fcntl.h>
#include <sys/stat.h>

int
main()
{
    Log::name("unit test");
    Log::debug();
    Log::foreground();
    Log::debug("De bug is in %s.", "de rug");
    Log::info("Thank %s for sharing %s.", "you", "that");
    Log::error("The %s in %s falls.", "rain", "Spain");
    if (open("/foo%bar", 0) < 0)
	Log::perror("/foo%c%s", '%', "bar");
    if (chmod("/", 0777) < 0)
	Log::error("%m on chmod(\"%s\", 0%o)", "/", 0777);
    return 0;
}

#endif /* UNIT_TEST_Log */

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

#ifndef Log_included
#define Log_included

#include "Boolean.h"
#include <stdarg.h>

//  Log is an augmented syslog(3C).  There are three log levels, DEBUG
//  INFO and LOG.  Logging can be done to the foreground (stderr) or
//  to the background (syslog).
//
//  Control interface:
//
//	debug(), info(), error()	control the log level.
//	foreground(), background()	control where log messages go.
//	name()				sets the program's name (i.e., fam)
//
//  Log interface:
//
//	debug(fmt, ...)		log at LOG_DEBUG level
//	info(fmt, ...)		log at LOG_INFO level
//	error(fmt, ...)		log at LOG_ERR level
//	critical(fmt, ...)	log at LOG_CRITICAL level
//	perror(fmt, ...)	log at LOG_ERR, append system error string
//	audit(bool, fmt, ...)   write message to security audit trail
//
//  All the log interface routines take printf-like arguments.  In
//  addition, all but audit() recognize "%m", which substitutes the system
//  error string.  "%m" works even when logging to foreground.
//
//  When NDEBUG is not defined, Log.C also implements a replacement
//  for the libc function __assert().  The replacement function writes
//  the failed assertion message to the current log (i.e., stderr or
//  syslog), then sets its uid to 0, creates a directory in /usr/tmp,
//  logs the name of the directory, and chdir() to it so the core file
//  can be found.

class Log {

public:

    enum LogLevel { CRITICAL = 0, ERROR = 1, INFO = 2, DEBUG = 3 };

#ifdef HAPPY_PURIFY
    Log();
    ~Log();
#endif

    // Message logging functions.  Four different priorities.

    static void debug(const char *fmt, ...);
    static void info(const char *fmt, ...);
    static void error(const char *fmt, ...);
    static void critical(const char *fmt, ...);

    //  logs regardless of log level
    static void log(LogLevel l, const char *fmt, ...);

    // perror() is like error() but appends system error string.

    static void perror(const char *format, ...);

    // Security audit trail messages.

    static void audit(bool success, const char *fmt, ...);

    // Control functions

    static void debug();
    static void info();
    static void error();
    static void disable_audit();

    static void foreground();
    static void background();

    static void name(const char *);
    static const char * getName() {return program_name;}

    static LogLevel get_level() { return level; }

private:

    static void vlog(LogLevel, const char *format, va_list);
    static void vfglog(const char *format, va_list args);

    static LogLevel level;
    static bool log_to_stderr;
    static const char *program_name;
    static bool syslog_open;
    static unsigned count;
#ifdef HAVE_AUDIT
    static bool audit_enabled;
#endif

};

#ifdef HAPPY_PURIFY
static Log Log_instance;
#endif

#endif /* !Log_included */

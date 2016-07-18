#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
//#include <fcntl.h>
#include "fam.h"
//#include "Boolean.h"
/* 

FILE test.c - simple fam test program

                 Usage: test [-r] [-f <file>] [-d <directory>]
		 ex.  test -f /etc/hosts -d /tmp

-r        includes request IDs in output (if you use this, put it first)
-f [file] monitors the given file
-d [dir]  monitors the given directory

*/


void processDirEvents(FAMEvent* fe);

bool suspend = false;
bool cont = false;
bool intr = false;
bool usr1 = false;
bool usr2 = false;
bool showReqIDs = false;
//int  lockfd = -1;
//FILE *fout;

void handle_stop(int) {
    printf("Suspended!\n");
    suspend = true;
}

void handle_cont(int) {
    printf("Resumed!\n");
    signal(SIGCONT, handle_cont);
    cont = true;
}

void handle_int(int) 
{
    printf("Interupted!\n");
    signal(SIGINT, handle_int);
    intr = true;
}

void handle_usr1(int) 
{
    printf("Got USR1!\n");
    signal(SIGUSR1, handle_usr1);
    usr1 = true;
}

void handle_usr2(int) 
{
    printf("Got USR2!\n");
    signal(SIGUSR2, handle_usr2);
    usr2 = true;
}

struct TestRequest
{
    FAMRequest fr;
    char path[PATH_MAX];
    char userData[PATH_MAX];
    bool isDir;
};

void sendRequests(FAMConnection &fc, TestRequest tr[], int trlen)
{
    for (int ii = 0; ii < trlen; ++ii) {
        if (tr[ii].isDir) {
	    printf("FAMMonitorDirectory(\"%s\")\n", tr[ii].path);
            if (FAMMonitorDirectory(&fc, tr[ii].path, &(tr[ii].fr), tr[ii].userData) < 0) {
                fprintf(stderr, "FAMMonitorDirectory error\n");
                exit(1);
            }
            if (showReqIDs) printf("req %d: ", tr[ii].fr.reqnum);
	    printf("FAMMonitorDirectory(\"%s\")\n", tr[ii].path);
        } else {
	    printf("FAMMonitorFile(\"%s\")\n", tr[ii].path);
            if (FAMMonitorFile(&fc, tr[ii].path, &(tr[ii].fr), tr[ii].userData) < 0) {
                fprintf(stderr, "FAMMonitorFile error\n");
                exit(1);
            }
            if (showReqIDs) printf("req %d: ", tr[ii].fr.reqnum);
	    printf("FAMMonitorFile(\"%s\")\n", tr[ii].path);
        }
    }
}

//int lprintf(const char *fmt, ...) {
//    int rv;
//    va_list args;
//    int lockfd = fileno(fout);
//
//    if (lockfd > 2) {
//fprintf(stderr, "locking fd %d...\n", lockfd);
//fflush(stderr);
//        lockf(lockfd, F_LOCK, 0);
//        lseek(lockfd, 0, SEEK_END);
//fprintf(stderr, "  locked!\n");
//fflush(stderr);
//    }
//    va_start(args, fmt);
//    rv = vfprintf(fout, fmt, args);
//    va_end(args);
//    fflush(fout);
//    if (lockfd > 2) {
//        lockf(lockfd, F_ULOCK, 0);
//    }
//    return rv;
//}

int
main(int argc, char **argv)
{
    /* Create fam test */
//    fout = stdout;
    FAMConnection fc;
    TestRequest tr[100];
    int requests = 0;
    FAMEvent fe;
    fd_set readfds;
    fd_set readfdscpy;
    setbuf(stdout, NULL);

    if (argc < 2 /*|| (argc %2 != 1)*/ ) {
	printf("usage: %s [-r] [-f <filename>] [-d <dirname>]\n", argv[0]);
	exit(1);
    }
    int arg;
    extern char *optarg;
    extern int optind;
    
    if(FAMOpen(&fc) < 0)
    {
        fprintf(stderr, "FAMOpen failed!\n");
        exit(1);
    }
    FD_ZERO(&readfds);
    FD_SET(fc.fd, &readfds);
//fcntl(STDIN_FILENO, F_SETFL, FNONBLK);
//FD_SET(STDIN_FILENO, &readfds);
    while ((arg = getopt(argc, argv, "f:d:r")) != -1) 
    {
        switch (arg) 
        {
        case 'd':
        case 'f':
            snprintf(tr[requests].userData, PATH_MAX, "%s %s: ",
                     (arg == 'd') ? "DIR " : "FILE", optarg);
            snprintf(tr[requests].path, PATH_MAX, "%s", optarg);
            tr[requests].isDir = (arg == 'd');
            ++requests;  // just don't use more than 100, OK?
            break;
        case 'r':
            showReqIDs = true;
            break;
        }
    }
    
    signal(SIGTSTP, handle_stop);
    signal(SIGCONT, handle_cont);
    signal(SIGINT, handle_int);
    signal(SIGUSR1, handle_usr1);
    signal(SIGUSR2, handle_usr2);
    
    sendRequests(fc, tr, requests);

    while (1) {
        if (suspend) {
            for (int ii  = 0; ii < requests; ii++) 
            {
                FAMSuspendMonitor(&fc, &(tr[ii].fr));
                printf("Suspended monitoring of request %i\n", ii);
            }
            fflush(stdout);
            suspend = false;
            kill(getpid(),SIGTSTP);
            signal(SIGTSTP, handle_stop);
        }
        if (cont) {
            for (int ii  = 0; ii < requests; ii++) 
            {
                FAMResumeMonitor(&fc, &(tr[ii].fr));
                printf("Resumed monitoring of request %i\n", ii);
            }
            fflush(stdout);
            cont = false;
        }
        if (intr) {
            // Cancel monitoring of every other request.  This makes
            // sure fam can handle the case where the connection goes
            // down with requests outstanding.
            for (int ii  = 0; ii < requests ; ii++) 
            {
                if (ii % 2 == 0) continue;
                FAMCancelMonitor(&fc, &(tr[ii].fr));
                printf("Canceled monitoring of request %i\n", ii);
            }
            FAMClose(&fc);
            exit(0);
        }
        if (usr1) {
            // Cancel all requests, close the connection, and reopen it.
            // This makes sure long-lived clients can connect, monitor, and
            // disconnect repeatedly without leaking.
            usr1 = false;
            int sleeptime = 1;
            for (int ii  = 0; ii < requests ; ii++)
            {
                FAMCancelMonitor(&fc, &(tr[ii].fr));
                printf("Canceled monitoring of request %i\n", ii);
            }
            FAMClose(&fc);
            printf("Closed connection, sleeping %d...\n", sleeptime);
            sleep(sleeptime);

            //  Now reconnect and resend the requests.
            if(FAMOpen(&fc) < 0)
            {
                fprintf(stderr, "FAMOpen failed!\n");
                exit(1);
            }
            FD_ZERO(&readfds);
            FD_SET(fc.fd, &readfds);
            sendRequests(fc, tr, requests);
        }
        if (usr2) {
            // Clean things up like a well-behaved client and exit.
            for (int ii  = 0; ii < requests ; ii++)
            {
                FAMCancelMonitor(&fc, &(tr[ii].fr));
                printf("Canceled monitoring of request %i\n", ii);
            }
            FAMClose(&fc);
            printf("Closed connection\n");
            exit(0);
        }

        readfdscpy = readfds;
        if (select(FD_SETSIZE, &readfdscpy, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) 
            {
                continue;
            }
            break;
        }
//        while (FAMPending(&fc) > 0) {
        for (; FAMPending(&fc); ) {
	    if (FAMNextEvent(&fc, &fe) < 0) {
                printf("Gahh, FAMNextEvent() returned < 0!\n");
                exit(1);
            }
	    processDirEvents(&fe);
            fflush(stdout);
	}
//	if (FD_ISSET(STDIN_FILENO, &readfdscpy)) {
//fprintf(stderr, "reading on stdin...\n");
//fflush(stderr);
//	    int readlen;
//            char buf[PATH_MAX];
//	    while ((readlen = read(STDIN_FILENO, buf, PATH_MAX - 1)) > 0) {
//fprintf(stderr, "  readlen == %d\n", readlen);
//fflush(stderr);
//		buf[readlen] = '\0';
//        	printf("%s", buf);
//            }
//fprintf(stderr, "  readlen == %d, errno == %d%s\n", readlen, errno, errno == EAGAIN ? " (EAGAIN)" : "");
//fflush(stderr);
//        }
    }
    return(EXIT_SUCCESS);
}



void
processDirEvents(FAMEvent* fe)
{
    if (fe->userdata) printf("%s  ", (char *)fe->userdata);
    if (showReqIDs) printf("req %d: ", fe->fr.reqnum);
    switch (fe->code) {
	case FAMExists:
	    printf("%s Exists\n", fe->filename);
	    break;
	case FAMEndExist:
	    printf("%s EndExist\n", fe->filename);
	    break;
	case FAMChanged:
	    printf("%s Changed\n", fe->filename);
	    break;
	case FAMDeleted:
	    printf("%s was deleted\n", fe->filename);
	    break;
	case FAMStartExecuting:
	    printf("%s started executing\n", fe->filename);
	    break;
	case FAMStopExecuting:
	    printf("%s stopped executing\n", fe->filename);
	    break;
	case FAMCreated:
	    printf("%s was created\n", fe->filename);
	    break;
	case FAMMoved:
	    printf("%s was moved\n", fe->filename);
	    break;
	default:
	    printf("(unknown event %d on %s)\n", fe->code, fe->filename);
    }
}


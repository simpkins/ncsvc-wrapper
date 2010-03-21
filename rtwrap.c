/*
 * This file intercepts SIOCADDRT and SIOCDELRT ioctl() calls.
 *
 * It silently ignores requests for specific routes.
 */
#define _GNU_SOURCE

#include <net/route.h>
#include <linux/sockios.h>
#include <arpa/inet.h>

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
 * Routes for the specified addresses are silently ignored.
 */
static char const *IGNORED_TARGETS[] =
{
    "10.12.34.0",

    /* A NULL entry marks the end of the list */
    NULL
};


static size_t f_num_ignored_targets;
static struct in_addr *f_ignored_target_addrs;
static FILE *f_logfile;

static int (*real_ioctl)(int, int, void*);

void librtwrap_init() __attribute__((constructor));

static void
dbg_log(char const *fmt, ...)
{
    va_list ap;

    if (f_logfile == NULL)
        return;

    va_start(ap, fmt);
    vfprintf(f_logfile, fmt, ap);
    va_end(ap);

    fprintf(f_logfile, "\n");
    fflush(f_logfile);
}

static void
fail(char const *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "rtwrap: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\nrtwrap: Aborting\n");
    abort();
}

static int
route_wrapper(int fd, int request, struct rtentry *entry)
{
    if (entry->rt_dst.sa_family != AF_INET)
        return real_ioctl(fd, request, entry);

    struct sockaddr_in const *dst_addr =
        (struct sockaddr_in const *)&entry->rt_dst;

    /*
     * Silently ignore requests to the specified targets
     */
    for (size_t n = 0; n < f_num_ignored_targets; ++n)
    {
        if (dst_addr->sin_addr.s_addr == f_ignored_target_addrs[n].s_addr)
        {
            dbg_log("ignoring request to %s route for %s",
                    request == SIOCADDRT ? "add" : "delete",
                    IGNORED_TARGETS[n]);
            return 0;
        }
    }

    return real_ioctl(fd, request, entry);
}

int
ioctl(int fd, int request, void *arg)
{
    if (request == SIOCADDRT || request == SIOCDELRT)
        return route_wrapper(fd, request, arg);

    return real_ioctl(fd, request, arg);
}

void
librtwrap_init()
{
    /*
     * Enable debug logging when the RTWRAP_LOG environment variable is set
     */
    if (getenv("RTWRAP_LOG") != NULL)
    {
        /*
         * We don't take the logfile path from the environment,
         * since that would be a giant security hole.
         */
        char const *LOGFILE_PATH = "/tmp/rtwrap.log";
        f_logfile = fopen(LOGFILE_PATH, "a");
        if (f_logfile == NULL)
            fail("failed to open debug log file \"%s\"", LOGFILE_PATH);
    }

    dbg_log("rtwrap starting at %ld", (long)time(NULL));

    /*
     * Unset the LD_PRELOAD environment variable.
     * Our wrapper library only needs to be loaded for ncsvc itself,
     * not other programs that it execs.
     *
     * (Furthermore, ncsvc is a 32-bit executable so our wrapper library is
     * too.  However, on 64-bit systems it execs some 64-bit programs, can't
     * load our 32-bit library, causing an error message from the loader.)
     */
    unsetenv("LD_PRELOAD");

    /*
     * Look up the address for the real ioctl() call
     */
    real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    if (real_ioctl == NULL)
        fail("failed to find real ioctl() function");

    /*
     * Convert strings in IGNORED_TARGETS to addresses
     */
    for (char const **p = IGNORED_TARGETS; *p != NULL; ++p)
        ++f_num_ignored_targets;

    f_ignored_target_addrs = malloc(sizeof(*f_ignored_target_addrs) *
                                    f_num_ignored_targets);
    if (f_ignored_target_addrs == NULL)
        fail("failed to allocate ignored addresses array");

    struct in_addr *addrp = f_ignored_target_addrs;
    for (char const **p = IGNORED_TARGETS; *p != NULL; ++p, ++addrp)
        inet_pton(AF_INET, *p, addrp);
}

/*
 * This is a small wrapper program to invoke ncsvc with the proper LD_PRELOAD
 * path so that our rtwrap library gets loaded to intercept ioctl() calls.
 */
#define _GNU_SOURCE

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD          INSTALL_DIR "/ncsvc.orig"
#define PRELOAD_LIB  INSTALL_DIR "/librtwrap.so"


bool
startswith(char const *haystack, char const *needle)
{
    return (strncmp(haystack, needle, strlen(needle)) == 0);
}

char *
concat(char const *s1, char const *s2)
{
    char *ret;
    size_t len1;
    size_t len2;

    len1 = strlen(s1);
    len2 = strlen(s2);

    ret = malloc(len1 + len2 + 1);
    if (ret == NULL)
        errx(1, "failed to allocate for concat");

    strcpy(ret, s1);
    strcpy(ret + len1, s2);
    ret[len1 + len2] = '\0';

    return ret;
}

char **
update_env(char *envp[], char const *preload_lib)
{
    size_t num_items;
    char **new_env;
    char **p;
    char **np;
    bool have_ld_preload = false;

    /*
     * Count the number of elements in the new environment
     * Start at 1, since we always set LD_PRELOAD
     */
    num_items = 1;
    for (p = envp; *p != NULL; ++p)
    {
        if (startswith(*p, "LD_PRELOAD="))
        {
            have_ld_preload = true;
            continue;
        }
        ++num_items;
    }

    new_env = malloc(num_items * sizeof(char*));
    if (new_env == NULL)
        errx(1, "failed to allocate new environment array");

    for (p = envp, np = new_env; *p != NULL; ++p, ++np)
    {
        if (startswith(*p, "LD_PRELOAD="))
        {
            /*
             * If we're running setuid or setgid, we shouldn't honor the
             * previous setting of LD_PRELOAD, for security reasons.
             *
             * For now simply always ignore the previous value.  (We could
             * check __libc_enable_secure if we want to see if it is safe to
             * use the old contents.  However, I haven't done so for now since
             * this is an internal glibc variable.)
             */
            *np = concat("LD_PRELOAD=", preload_lib);
        }
        else
        {
            *np = *p;
        }
    }

    if (!have_ld_preload)
    {
        *np = concat("LD_PRELOAD=", preload_lib);
        ++np;
    }

    *np = NULL;

    return new_env;
}

int
main(int argc, char *argv[], char *envp[])
{
    char **new_env;

    /*
     * argv should always be NULL terminated, but check to make sure
     */
    if (argv[argc] != NULL)
        errx(1, "argv is not NULL terminated");

    new_env = update_env(envp, PRELOAD_LIB);

    /*
     * Set the UID and GID to 0
     * Otherwise ld.so will ignore LD_PRELOAD.
     */
    setreuid(0, 0);
    setregid(0, 0);

    execve(CMD, argv, new_env);

    err(1, "execve failed");
    return 1;
}

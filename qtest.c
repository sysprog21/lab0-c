/* Implementation of testing code for queue code */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "dudect/fixture.h"

/* Our program needs to use regular malloc/free */
#define INTERNAL 1
#include "harness.h"

/* What character limit will be used for displaying strings? */
#define MAXSTRING 1024

/* How much padding should be added to check for string overrun? */
#define STRINGPAD MAXSTRING

/*
 * It is a bit sketchy to use this #include file on the solution version of the
 * code.
 * OK as long as head field of queue_t structure is in first position in
 * solution code
 */
#include "queue.h"

#include "console.h"
#include "report.h"

/* Settable parameters */

#define HISTORY_LEN 20

/*
 * How large is a queue before it's considered big.
 * This affects how it gets printed
 * and whether cautious mode is used when freeing the list
 */
#define BIG_QUEUE 30
static int big_queue_size = BIG_QUEUE;

/* Global variables */

/* Queue being tested */
static queue_t *q = NULL;

/* Number of elements in queue */
static size_t qcnt = 0;

/* How many times can queue operations fail */
static int fail_limit = BIG_QUEUE;
static int fail_count = 0;

static int string_length = MAXSTRING;

#define MIN_RANDSTR_LEN 5
#define MAX_RANDSTR_LEN 10
static const char charset[] = "abcdefghijklmnopqrstuvwxyz";

/* Forward declarations */
static bool show_queue(int vlevel);
static bool do_new(int argc, char *argv[]);
static bool do_free(int argc, char *argv[]);
static bool do_insert_head(int argc, char *argv[]);
static bool do_insert_tail(int argc, char *argv[]);
static bool do_remove_head(int argc, char *argv[]);
static bool do_remove_head_quiet(int argc, char *argv[]);
static bool do_reverse(int argc, char *argv[]);
static bool do_size(int argc, char *argv[]);
static bool do_sort(int argc, char *argv[]);
static bool do_show(int argc, char *argv[]);

static void queue_init();

static void console_init()
{
    add_cmd("new", do_new, "                | Create new queue");
    add_cmd("free", do_free, "                | Delete queue");
    add_cmd("ih", do_insert_head,
            " str [n]        | Insert string str at head of queue n times. "
            "Generate random string(s) if str equals RAND. (default: n == 1)");
    add_cmd("it", do_insert_tail,
            " str [n]        | Insert string str at tail of queue n times. "
            "Generate random string(s) if str equals RAND. (default: n == 1)");
    add_cmd("rh", do_remove_head,
            " [str]          | Remove from head of queue.  Optionally compare "
            "to expected value str");
    add_cmd(
        "rhq", do_remove_head_quiet,
        "                | Remove from head of queue without reporting value.");
    add_cmd("reverse", do_reverse, "                | Reverse queue");
    add_cmd("sort", do_sort, "                | Sort queue in ascending order");
    add_cmd("size", do_size,
            " [n]            | Compute queue size n times (default: n == 1)");
    add_cmd("show", do_show, "                | Show queue contents");
    add_param("length", &string_length, "Maximum length of displayed string",
              NULL);
    add_param("malloc", &fail_probability, "Malloc failure probability percent",
              NULL);
    add_param("fail", &fail_limit,
              "Number of times allow queue operations to return false", NULL);
}

static bool do_new(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (q) {
        report(3, "Freeing old queue");
        ok = do_free(argc, argv);
    }
    error_check();

    if (exception_setup(true))
        q = q_new();
    exception_cancel();
    qcnt = 0;
    show_queue(3);

    return ok && !error_check();
}

static bool do_free(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (!q)
        report(3, "Warning: Calling free on null queue");
    error_check();

    if (qcnt > big_queue_size)
        set_cautious_mode(false);
    if (exception_setup(true))
        q_free(q);
    exception_cancel();
    set_cautious_mode(true);

    q = NULL;
    qcnt = 0;
    show_queue(3);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        ok = false;
    }

    return ok && !error_check();
}
/*
 * TODO: Add a buf_size check of if the buf_size may be less
 * than MIN_RANDSTR_LEN.
 */
static void fill_rand_string(char *buf, size_t buf_size)
{
    size_t len = 0;
    while (len < MIN_RANDSTR_LEN)
        len = rand() % buf_size;

    for (size_t n = 0; n < len; n++) {
        buf[n] = charset[rand() % (sizeof charset - 1)];
    }
    buf[len] = '\0';
}

static bool do_insert_head(int argc, char *argv[])
{
    char *lasts = NULL;
    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!q)
        report(3, "Warning: Calling insert head on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_head(q, inserts);
            if (rval) {
                qcnt++;
                if (!q->head->value) {
                    report(1, "ERROR: Failed to save copy of string in list");
                    ok = false;
                } else if (r == 0 && inserts == q->head->value) {
                    report(1,
                           "ERROR: Need to allocate and copy string for new "
                           "list element");
                    ok = false;
                    break;
                } else if (r == 1 && lasts == q->head->value) {
                    report(1,
                           "ERROR: Need to allocate separate string for each "
                           "list element");
                    ok = false;
                    break;
                }
                lasts = q->head->value;
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    show_queue(3);
    return ok;
}

static bool do_insert_tail(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_insert_tail_const();
        if (!ok) {
            report(1, "ERROR: Probably not constant time");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!q)
        report(3, "Warning: Calling insert tail on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_tail(q, inserts);
            if (rval) {
                qcnt++;
                if (!q->head->value) {
                    report(1, "ERROR: Failed to save copy of string in list");
                    ok = false;
                }
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();
    show_queue(3);
    return ok;
}

static bool do_remove_head(int argc, char *argv[])
{
    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    char *removes = malloc(string_length + STRINGPAD + 1);
    if (!removes) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        return false;
    }

    char *checks = malloc(string_length + 1);
    if (!checks) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        free(removes);
        return false;
    }

    bool check = argc > 1;
    bool ok = true;
    if (check) {
        strncpy(checks, argv[1], string_length + 1);
        checks[string_length] = '\0';
    }

    removes[0] = '\0';
    memset(removes + 1, 'X', string_length + STRINGPAD - 1);
    removes[string_length + STRINGPAD] = '\0';

    if (!q)
        report(3, "Warning: Calling remove head on null queue");
    else if (!q->head)
        report(3, "Warning: Calling remove head on empty queue");
    error_check();

    bool rval = false;
    if (exception_setup(true))
        rval = q_remove_head(q, removes, string_length + 1);
    exception_cancel();

    if (rval) {
        removes[string_length + STRINGPAD] = '\0';
        if (removes[0] == '\0') {
            report(1, "ERROR: Failed to store removed value");
            ok = false;
        }

        /*
         * Check whether padding in array removes are still initial value 'X'.
         * If there's other character in padding, it's overflowed.
         */
        int i = string_length + 1;
        while ((i < string_length + STRINGPAD) && (removes[i] == 'X'))
            i++;
        if (i != string_length + STRINGPAD) {
            report(1,
                   "ERROR: copying of string in remove_head overflowed "
                   "destination buffer.");
            ok = false;
        } else {
            report(2, "Removed %s from queue", removes);
        }
        qcnt--;
    } else {
        fail_count++;
        if (!check && fail_count < fail_limit) {
            report(2, "Removal from queue failed");
        } else {
            report(1, "ERROR: Removal from queue failed (%d failures total)",
                   fail_count);
            ok = false;
        }
    }

    if (ok && check && strcmp(removes, checks)) {
        report(1, "ERROR: Removed value %s != expected value %s", removes,
               checks);
        ok = false;
    }

    show_queue(3);

    free(removes);
    free(checks);
    return ok && !error_check();
}

static bool do_remove_head_quiet(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (!q)
        report(3, "Warning: Calling remove head on null queue");
    else if (!q->head)
        report(3, "Warning: Calling remove head on empty queue");
    error_check();

    bool rval = false;
    if (exception_setup(true))
        rval = q_remove_head(q, NULL, 0);
    exception_cancel();

    if (rval) {
        report(2, "Removed element from queue");
        qcnt--;
    } else {
        fail_count++;
        if (fail_count < fail_limit)
            report(2, "Removal failed");
        else {
            report(1, "ERROR: Removal failed (%d failures total)", fail_count);
            ok = false;
        }
    }

    show_queue(3);
    return ok && !error_check();
}

static bool do_reverse(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!q)
        report(3, "Warning: Calling reverse on null queue");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_reverse(q);
    exception_cancel();

    set_noallocate_mode(false);
    show_queue(3);
    return !error_check();
}

static bool do_size(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_size_const();
        if (!ok) {
            report(1, "ERROR: Probably not constant time");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    if (argc != 1 && argc != 2) {
        report(1, "%s takes 0-1 arguments", argv[0]);
        return false;
    }

    int reps = 1;
    bool ok = true;
    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    if (argc == 2) {
        if (!get_int(argv[1], &reps)) {
            report(1, "Invalid number of calls to size '%s'", argv[2]);
        }
    }

    int cnt = 0;
    if (!q)
        report(3, "Warning: Calling size on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            cnt = q_size(q);
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (ok) {
        if (qcnt == cnt) {
            report(2, "Queue size = %d", cnt);
        } else {
            report(1,
                   "ERROR: Computed queue size as %d, but correct value is %d",
                   cnt, (int) qcnt);
            ok = false;
        }
    }

    show_queue(3);

    return ok && !error_check();
}

bool do_sort(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!q)
        report(3, "Warning: Calling sort on null queue");
    error_check();

    int cnt = q_size(q);
    if (cnt < 2)
        report(3, "Warning: Calling sort on single node");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_sort(q);
    exception_cancel();
    set_noallocate_mode(false);

    bool ok = true;
    if (q) {
        for (list_ele_t *e = q->head; e && --cnt; e = e->next) {
            /* Ensure each element in ascending order */
            /* FIXME: add an option to specify sorting order */
            if (strcasecmp(e->value, e->next->value) > 0) {
                report(1, "ERROR: Not sorted in ascending order");
                ok = false;
                break;
            }
        }
    }

    show_queue(3);
    return ok && !error_check();
}

static bool show_queue(int vlevel)
{
    bool ok = true;
    if (verblevel < vlevel)
        return true;

    int cnt = 0;
    if (!q) {
        report(vlevel, "q = NULL");
        return true;
    }

    report_noreturn(vlevel, "q = [");
    list_ele_t *e = q->head;
    if (exception_setup(true)) {
        while (ok && e && cnt < qcnt) {
            if (cnt < big_queue_size)
                report_noreturn(vlevel, cnt == 0 ? "%s" : " %s", e->value);
            e = e->next;
            cnt++;
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (!ok) {
        report(vlevel, " ... ]");
        return false;
    }

    if (!e) {
        if (cnt <= big_queue_size)
            report(vlevel, "]");
        else
            report(vlevel, " ... ]");
    } else {
        report(vlevel, " ... ]");
        report(
            vlevel,
            "ERROR:  Either list has cycle, or queue has more than %d elements",
            qcnt);
        ok = false;
    }

    return ok;
}

static bool do_show(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }
    return show_queue(0);
}

/* Signal handlers */
static void sigsegvhandler(int sig)
{
    report(1,
           "Segmentation fault occurred.  You dereferenced a NULL or invalid "
           "pointer");
    /* Raising a SIGABRT signal to produce a core dump for debugging. */
    abort();
}

static void sigalrmhandler(int sig)
{
    trigger_exception(
        "Time limit exceeded.  Either you are in an infinite loop, or your "
        "code is too inefficient");
}

static void queue_init()
{
    fail_count = 0;
    q = NULL;
    signal(SIGSEGV, sigsegvhandler);
    signal(SIGALRM, sigalrmhandler);
}

static bool queue_quit(int argc, char *argv[])
{
    report(3, "Freeing queue");
    if (qcnt > big_queue_size)
        set_cautious_mode(false);

    if (exception_setup(true))
        q_free(q);
    exception_cancel();
    set_cautious_mode(true);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        return false;
    }

    return true;
}

static void usage(char *cmd)
{
    printf("Usage: %s [-h] [-f IFILE][-v VLEVEL][-l LFILE]\n", cmd);
    printf("\t-h         Print this information\n");
    printf("\t-f IFILE   Read commands from IFILE\n");
    printf("\t-v VLEVEL  Set verbosity level\n");
    printf("\t-l LFILE   Echo results to LFILE\n");
    exit(0);
}

#define GIT_HOOK ".git/hooks/"
static bool sanity_check()
{
    struct stat buf;
    /* Directory .git not found */
    if (stat(".git", &buf)) {
        fprintf(stderr,
                "FATAL: You should run qtest in the directory containing valid "
                "git workspace.\n");
        return false;
    }
    /* Expected pre-commit and pre-push hooks not found */
    if (stat(GIT_HOOK "commit-msg", &buf) ||
        stat(GIT_HOOK "pre-commit", &buf) || stat(GIT_HOOK "pre-push", &buf)) {
        fprintf(stderr, "FATAL: Git hooks are not properly installed.\n");

        /* Attempt to install Git hooks */
        char *argv[] = {"sh", "-c", "scripts/install-git-hooks", NULL};
        extern char **environ;
        pid_t pid;
        int status = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
        if (status == 0) {
            /* Finally, succeed */
            if ((waitpid(pid, &status, 0) != -1) && (status == 0))
                return true;
            perror("waitpid");
        }
        return false;
    }
    return true;
}


#define BUFSIZE 256
int main(int argc, char *argv[])
{
    /* sanity check for git hook integration */
    if (!sanity_check())
        return -1;

    /* To hold input file name */
    char buf[BUFSIZE];
    char *infile_name = NULL;
    char lbuf[BUFSIZE];
    char *logfile_name = NULL;
    int level = 4;
    int c;

    while ((c = getopt(argc, argv, "hv:f:l:")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            break;
        case 'f':
            strncpy(buf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            infile_name = buf;
            break;
        case 'v': {
            char *endptr;
            errno = 0;
            level = strtol(optarg, &endptr, 10);
            if (errno != 0 || endptr == optarg) {
                fprintf(stderr, "Invalid verbosity level\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'l':
            strncpy(lbuf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            logfile_name = lbuf;
            break;
        default:
            printf("Unknown option '%c'\n", c);
            usage(argv[0]);
            break;
        }
    }

    srand((unsigned int) (time(NULL)));
    queue_init();
    init_cmd();
    console_init();

    /* Trigger call back function(auto completion) */
    linenoiseSetCompletionCallback(completion);

    linenoiseHistorySetMaxLen(HISTORY_LEN);
    linenoiseHistoryLoad(HISTORY_FILE); /* Load the history at startup */
    set_verblevel(level);
    if (level > 1) {
        set_echo(true);
    }
    if (logfile_name)
        set_logfile(logfile_name);

    add_quit_helper(queue_quit);

    bool ok = true;
    ok = ok && run_console(infile_name);
    ok = ok && finish_cmd();

    return ok ? 0 : 1;
}

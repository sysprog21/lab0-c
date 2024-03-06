/* Implementation of simple command-line interface */

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include "console.h"
#include "report.h"
#include "web.h"

/* Some global values */
int simulation = 0;
int show_entropy = 0;
static cmd_element_t *cmd_list = NULL;
static param_element_t *param_list = NULL;
static bool block_flag = false;
static bool prompt_flag = true;

/* Am I timing a command that has the console blocked? */
static bool block_timing = false;

/* Time of day */
static double first_time, last_time;

/* Implement buffered I/O using variant of RIO package from CS:APP
 * Must create stack of buffers to handle I/O with nested source commands.
 */

#define RIO_BUFSIZE 8192

typedef struct __rio {
    int fd;                /* File descriptor */
    int count;             /* Unread bytes in internal buffer */
    char *bufptr;          /* Next unread byte in internal buffer */
    char buf[RIO_BUFSIZE]; /* Internal buffer */
    struct __rio *prev;    /* Next element in stack */
} rio_t;

static rio_t *buf_stack;
static char linebuf[RIO_BUFSIZE];

/* Maximum file descriptor */
static int fd_max = 0;

/* Parameters */
static int err_limit = 5;
static int err_cnt = 0;
static int echo = 0;

static bool quit_flag = false;
static char *prompt = "cmd> ";
static bool has_infile = false;

/* Optional function to call as part of exit process */
/* Maximum number of quit functions */

#define MAXQUIT 10
static cmd_func_t quit_helpers[MAXQUIT];
static int quit_helper_cnt = 0;

static void init_in();

static bool push_file(char *fname);
static void pop_file();

static bool interpret_cmda(int argc, char *argv[]);

/* Add a new command */
void add_cmd(char *name, cmd_func_t operation, char *summary, char *param)
{
    cmd_element_t *next_cmd = cmd_list;
    cmd_element_t **last_loc = &cmd_list;
    while (next_cmd && strcmp(name, next_cmd->name) > 0) {
        last_loc = &next_cmd->next;
        next_cmd = next_cmd->next;
    }

    cmd_element_t *cmd = malloc_or_fail(sizeof(cmd_element_t), "add_cmd");
    cmd->name = name;
    cmd->operation = operation;
    cmd->summary = summary;
    cmd->param = param;
    cmd->next = next_cmd;
    *last_loc = cmd;
}

/* Add a new parameter */
void add_param(char *name, int *valp, char *summary, setter_func_t setter)
{
    param_element_t *next_param = param_list;
    param_element_t **last_loc = &param_list;
    while (next_param && strcmp(name, next_param->name) > 0) {
        last_loc = &next_param->next;
        next_param = next_param->next;
    }

    param_element_t *param =
        malloc_or_fail(sizeof(param_element_t), "add_param");
    param->name = name;
    param->valp = valp;
    param->summary = summary;
    param->setter = setter;
    param->next = next_param;
    *last_loc = param;
}

/* Parse a string into a command line */
static char **parse_args(char *line, int *argcp)
{
    /* Must first determine how many arguments there are.
     * Replace all white space with null characters
     */
    size_t len = strlen(line);

    /* First copy into buffer with each substring null-terminated */
    char *buf = malloc_or_fail(len + 1, "parse_args");
    buf[len] = '\0';

    char *src = line;
    char *dst = buf;
    bool skipping = true;
    int c;
    int argc = 0;
    while ((c = *src++) != '\0') {
        if (isspace(c)) {
            if (!skipping) {
                /* Hit end of word */
                *dst++ = '\0';
                skipping = true;
            }
        } else {
            if (skipping) {
                /* Hit start of new word */
                argc++;
                skipping = false;
            }
            *dst++ = c;
        }
    }

    /* Now assemble into array of strings */
    char **argv = calloc_or_fail(argc, sizeof(char *), "parse_args");
    src = buf;
    for (int i = 0; i < argc; i++) {
        argv[i] = strsave_or_fail(src, "parse_args");
        src += strlen(argv[i]) + 1;
    }

    free_block(buf, len + 1);
    *argcp = argc;
    return argv;
}

static void record_error()
{
    err_cnt++;
    if (err_cnt >= err_limit) {
        report(1, "Error limit exceeded.  Stopping command execution");
        quit_flag = true;
    }
}

/* Execute a command that has already been split into arguments */
static bool interpret_cmda(int argc, char *argv[])
{
    if (argc == 0)
        return true;
    /* Try to find matching command */
    cmd_element_t *next_cmd = cmd_list;
    bool ok = true;
    while (next_cmd && strcmp(argv[0], next_cmd->name) != 0)
        next_cmd = next_cmd->next;
    if (next_cmd) {
        ok = next_cmd->operation(argc, argv);
        if (!ok)
            record_error();
    } else {
        report(1, "Unknown command '%s'", argv[0]);
        record_error();
        ok = false;
    }

    return ok;
}

/* Execute a command from a command line */
static bool interpret_cmd(char *cmdline)
{
    if (quit_flag)
        return false;

    int argc;
    char **argv = parse_args(cmdline, &argc);
    bool ok = interpret_cmda(argc, argv);
    for (int i = 0; i < argc; i++)
        free_string(argv[i]);
    free_array(argv, argc, sizeof(char *));

    return ok;
}

/* Set function to be executed as part of program exit */
void add_quit_helper(cmd_func_t qf)
{
    if (quit_helper_cnt < MAXQUIT)
        quit_helpers[quit_helper_cnt++] = qf;
    else
        report_event(MSG_FATAL, "Exceeded limit on quit helpers");
}

/* Turn echoing on/off */
void set_echo(bool on)
{
    echo = on ? 1 : 0;
}

/* Built-in commands */
static bool do_quit(int argc, char *argv[])
{
    cmd_element_t *c = cmd_list;
    bool ok = true;
    while (c) {
        cmd_element_t *ele = c;
        c = c->next;
        free_block(ele, sizeof(cmd_element_t));
    }

    param_element_t *p = param_list;
    while (p) {
        param_element_t *ele = p;
        p = p->next;
        free_block(ele, sizeof(param_element_t));
    }

    while (buf_stack)
        pop_file();

    for (int i = 0; i < quit_helper_cnt; i++) {
        ok = ok && quit_helpers[i](argc, argv);
    }

    quit_flag = true;
    return ok;
}

static bool do_help(int argc, char *argv[])
{
    cmd_element_t *clist = cmd_list;
    report(1, "Commands:", argv[0]);
    while (clist) {
        report(1, "  %-12s%-12s | %s", clist->name, clist->param,
               clist->summary);
        clist = clist->next;
    }
    param_element_t *plist = param_list;
    report(1, "Options:");
    while (plist) {
        report(1, "  %-12s%-12d | %s", plist->name, *plist->valp,
               plist->summary);
        plist = plist->next;
    }
    return true;
}

static bool do_comment_cmd(int argc, char *argv[])
{
    if (echo)
        return true;

    int i;
    for (i = 0; i < argc - 1; i++)
        report_noreturn(1, "%s ", argv[i]);
    if (i < argc)
        report(1, "%s", argv[i]);

    return true;
}

/* Extract integer from text and store at loc */
bool get_int(char *vname, int *loc)
{
    char *end = NULL;
    long int v = strtol(vname, &end, 0);
    if (v == LONG_MIN || *end != '\0')
        return false;

    *loc = (int) v;
    return true;
}

static bool do_option(int argc, char *argv[])
{
    if (argc == 1) {
        param_element_t *plist = param_list;
        report(1, "Options:");
        while (plist) {
            report(1, "  %-12s%-12d | %s", plist->name, *plist->valp,
                   plist->summary);
            plist = plist->next;
        }
        return true;
    }

    for (int i = 1; i < argc; i++) {
        char *name = argv[i];
        int value = 0;
        bool found = false;
        /* Get value from next argument */
        if (i + 1 >= argc) {
            report(1, "No value given for parameter %s", name);
            return false;
        } else if (!get_int(argv[++i], &value)) {
            report(1, "Cannot parse '%s' as integer", argv[i]);
            return false;
        }
        /* Find parameter in list */
        param_element_t *plist = param_list;
        while (!found && plist) {
            if (strcmp(plist->name, name) == 0) {
                int oldval = *plist->valp;
                *plist->valp = value;
                if (plist->setter)
                    plist->setter(oldval);
                found = true;
            } else
                plist = plist->next;
        }
        /* Didn't find parameter */
        if (!found) {
            report(1, "Unknown parameter '%s'", name);
            return false;
        }
    }

    return true;
}

static bool do_source(int argc, char *argv[])
{
    if (argc < 2) {
        report(1, "No source file given");
        return false;
    }

    if (!push_file(argv[1])) {
        report(1, "Could not open source file '%s'", argv[1]);
        return false;
    }

    return true;
}

static bool do_log(int argc, char *argv[])
{
    if (argc < 2) {
        report(1, "No log file given");
        return false;
    }

    bool result = set_logfile(argv[1]);
    if (!result)
        report(1, "Couldn't open log file '%s'", argv[1]);

    return result;
}

static bool do_time(int argc, char *argv[])
{
    double delta = delta_time(&last_time);
    bool ok = true;
    if (argc <= 1) {
        double elapsed = last_time - first_time;
        report(1, "Elapsed time = %.3f, Delta time = %.3f", elapsed, delta);
    } else {
        ok = interpret_cmda(argc - 1, argv + 1);
        if (block_flag) {
            block_timing = true;
        } else {
            delta = delta_time(&last_time);
            report(1, "Delta time = %.3f", delta);
        }
    }

    return ok;
}

static bool use_linenoise = true;
static int web_fd;

static bool do_web(int argc, char *argv[])
{
    int port = 9999;
    if (argc == 2) {
        if (argv[1][0] >= '0' && argv[1][0] <= '9')
            port = atoi(argv[1]);
    }

    web_fd = web_open(port);
    if (web_fd > 0) {
        printf("listen on port %d, fd is %d\n", port, web_fd);
        line_set_eventmux_callback(web_eventmux);
        use_linenoise = false;
    } else {
        perror("ERROR");
        exit(web_fd);
    }
    return true;
}

/* Initialize interpreter */
void init_cmd()
{
    cmd_list = NULL;
    param_list = NULL;
    err_cnt = 0;
    quit_flag = false;

    ADD_COMMAND(help, "Show summary", "");
    ADD_COMMAND(option,
                "Display or set options. See 'Options' section for details",
                "[name val]");
    ADD_COMMAND(quit, "Exit program", "");
    ADD_COMMAND(source, "Read commands from source file", "");
    ADD_COMMAND(log, "Copy output to file", "file");
    ADD_COMMAND(time, "Time command execution", "cmd arg ...");
    ADD_COMMAND(web, "Read commands from builtin web server", "[port]");
    add_cmd("#", do_comment_cmd, "Display comment", "...");
    add_param("simulation", &simulation, "Start/Stop simulation mode", NULL);
    add_param("verbose", &verblevel, "Verbosity level", NULL);
    add_param("error", &err_limit, "Number of errors until exit", NULL);
    add_param("echo", &echo, "Do/don't echo commands", NULL);
    add_param("entropy", &show_entropy, "Show/Hide Shannon entropy", NULL);

    init_in();
    init_time(&last_time);
    first_time = last_time;
}

/* Create new buffer for named file.
 * Name == NULL for stdin.
 * Return true if successful.
 */
static bool push_file(char *fname)
{
    int fd = fname ? open(fname, O_RDONLY) : STDIN_FILENO;
    has_infile = fname ? true : false;
    if (fd < 0)
        return false;

    if (fd > fd_max)
        fd_max = fd;

    rio_t *rnew = malloc_or_fail(sizeof(rio_t), "push_file");
    rnew->fd = fd;
    rnew->count = 0;
    rnew->bufptr = rnew->buf;
    rnew->prev = buf_stack;
    buf_stack = rnew;

    return true;
}

/* Pop a file buffer from stack */
static void pop_file()
{
    if (buf_stack) {
        rio_t *rsave = buf_stack;
        buf_stack = rsave->prev;
        close(rsave->fd);
        free_block(rsave, sizeof(rio_t));
    }
}

/* Handling of input */
static void init_in()
{
    buf_stack = NULL;
}

/* Read command from input file.
 * When hit EOF, close that file and return NULL
 */
static char *readline()
{
    char c;
    char *lptr = linebuf;

    if (!buf_stack)
        return NULL;

    for (int cnt = 0; cnt < RIO_BUFSIZE - 2; cnt++) {
        if (buf_stack->count <= 0) {
            /* Need to read from input file */
            buf_stack->count = read(buf_stack->fd, buf_stack->buf, RIO_BUFSIZE);
            buf_stack->bufptr = buf_stack->buf;
            if (buf_stack->count <= 0) {
                /* Encountered EOF */
                pop_file();
                if (cnt > 0) {
                    /* Last line of file did not terminate with newline. */
                    /*  Terminate line & return it */
                    *lptr++ = '\n';
                    *lptr++ = '\0';
                    if (echo) {
                        report_noreturn(1, prompt);
                        report_noreturn(1, linebuf);
                    }
                    return linebuf;
                }
                return NULL;
            }
        }

        /* Have text in buffer */
        c = *buf_stack->bufptr++;
        *lptr++ = c;
        buf_stack->count--;
        if (c == '\n')
            break;
    }

    if (c != '\n') {
        /* Hit buffer limit.  Artificially terminate line */
        *lptr++ = '\n';
    }
    *lptr++ = '\0';

    if (echo) {
        report_noreturn(1, prompt);
        report_noreturn(1, linebuf);
    }

    return linebuf;
}

static bool cmd_done()
{
    return !buf_stack || quit_flag;
}

/* Handle command processing in program that uses select as main control loop.
 * Like select, but checks whether command input either present in internal
 * buffer
 * or readable from command input.  If so, that command is executed.
 * Same return as select.  Command input file removed from readfds
 *
 * nfds should be set to the maximum file descriptor for network sockets.
 * If nfds == 0, this indicates that there is no pending network activity
 */
int web_connfd;
static int cmd_select(int nfds,
                      fd_set *readfds,
                      fd_set *writefds,
                      fd_set *exceptfds,
                      struct timeval *timeout)
{
    fd_set local_readset;

    if (cmd_done())
        return 0;

    if (!block_flag) {
        int infd;
        /* Process any commands in input buffer */
        if (!readfds)
            readfds = &local_readset;

        /* Add input fd to readset for select */
        infd = buf_stack->fd;
        FD_ZERO(readfds);
        FD_SET(infd, readfds);

        /* If web not ready listen */
        if (web_fd != -1)
            FD_SET(web_fd, readfds);

        if (infd == STDIN_FILENO && prompt_flag) {
            char *cmdline = linenoise(prompt);
            if (cmdline)
                interpret_cmd(cmdline);
            fflush(stdout);
            prompt_flag = true;
        } else if (infd != STDIN_FILENO) {
            char *cmdline = readline();
            if (cmdline)
                interpret_cmd(cmdline);
        }
    }
    return 0;
}

bool finish_cmd()
{
    bool ok = true;
    if (!quit_flag)
        ok = ok && do_quit(0, NULL);
    has_infile = false;
    return ok && err_cnt == 0;
}

static bool cmd_maybe(const char *target, const char *src)
{
    for (int i = 0; i < strlen(src); i++) {
        if (target[i] == '\0')
            return false;
        if (src[i] != target[i])
            return false;
    }
    return true;
}

void completion(const char *buf, line_completions_t *lc)
{
    if (strncmp("option ", buf, 7) == 0) {
        param_element_t *plist = param_list;

        while (plist) {
            char str[128] = "";
            /* if parameter is too long, now we just ignore it */
            if (strlen(plist->name) > 120)
                continue;

            strcat(str, "option ");
            strcat(str, plist->name);
            if (cmd_maybe(str, buf))
                line_add_completion(lc, str);

            plist = plist->next;
        }
        return;
    }

    cmd_element_t *clist = cmd_list;
    while (clist) {
        if (cmd_maybe(clist->name, buf))
            line_add_completion(lc, clist->name);

        clist = clist->next;
    }
}

bool run_console(char *infile_name)
{
    if (!push_file(infile_name)) {
        report(1, "ERROR: Could not open source file '%s'", infile_name);
        return false;
    }

    if (!has_infile) {
        char *cmdline;
        while (use_linenoise && (cmdline = linenoise(prompt))) {
            interpret_cmd(cmdline);
            line_history_add(cmdline);       /* Add to the history. */
            line_history_save(HISTORY_FILE); /* Save the history on disk. */
            line_free(cmdline);
            while (buf_stack && buf_stack->fd != STDIN_FILENO)
                cmd_select(0, NULL, NULL, NULL, NULL);
            has_infile = false;
        }
        if (!use_linenoise) {
            while (!cmd_done())
                cmd_select(0, NULL, NULL, NULL, NULL);
        }
    } else {
        while (!cmd_done())
            cmd_select(0, NULL, NULL, NULL, NULL);
    }

    return err_cnt == 0;
}

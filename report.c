#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "report.h"
#include "web.h"

#define MAX(a, b) ((a) < (b) ? (b) : (a))

static FILE *errfile = NULL;
static FILE *verbfile = NULL;
static FILE *logfile = NULL;

int verblevel = 0;
static void init_files(FILE *efile, FILE *vfile)
{
    errfile = efile;
    verbfile = vfile;
}

static char fail_buf[1024] = "FATAL Error.  Exiting\n";

static volatile int ret = 0;

/* Default fatal function */
static void default_fatal_fun()
{
    ret = write(STDOUT_FILENO, fail_buf, strlen(fail_buf) + 1);
    if (logfile)
        fputs(fail_buf, logfile);
}

/* Optional function to call when fatal error encountered */
static void (*fatal_fun)() = default_fatal_fun;

void set_verblevel(int level)
{
    verblevel = level;
}

bool set_logfile(char *file_name)
{
    logfile = fopen(file_name, "w");
    return logfile != NULL;
}

void report_event(message_t msg, char *fmt, ...)
{
    va_list ap;
    bool fatal = msg == MSG_FATAL;
    // cppcheck-suppress constVariable
    static char *msg_name_text[N_MSG] = {
        "WARNING",
        "ERROR",
        "FATAL ERROR",
    };
    char *msg_name = msg_name_text[2];
    if (msg < N_MSG)
        msg_name = msg_name_text[msg];
    int level = N_MSG - msg - 1;
    if (verblevel < level)
        return;

    if (!errfile)
        init_files(stdout, stdout);

    va_start(ap, fmt);
    fprintf(errfile, "%s: ", msg_name);
    vfprintf(errfile, fmt, ap);
    fprintf(errfile, "\n");
    fflush(errfile);
    va_end(ap);

    if (logfile) {
        va_start(ap, fmt);
        fprintf(logfile, "Error: ");
        vfprintf(logfile, fmt, ap);
        fprintf(logfile, "\n");
        fflush(logfile);
        va_end(ap);
        fclose(logfile);
    }

    if (fatal) {
        if (fatal_fun)
            fatal_fun();
        exit(1);
    }
}

#define BUF_SIZE 4096
extern int web_connfd;
void report(int level, char *fmt, ...)
{
    if (!verbfile)
        init_files(stdout, stdout);

    char buffer[BUF_SIZE];
    if (level <= verblevel) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(verbfile, fmt, ap);
        fprintf(verbfile, "\n");
        fflush(verbfile);
        va_end(ap);

        if (logfile) {
            va_start(ap, fmt);
            vfprintf(logfile, fmt, ap);
            fprintf(logfile, "\n");
            fflush(logfile);
            va_end(ap);
        }
        va_start(ap, fmt);
        vsnprintf(buffer, BUF_SIZE, fmt, ap);
        va_end(ap);
    }
    if (web_connfd) {
        int len = strlen(buffer);
        buffer[len] = '\n';
        buffer[len + 1] = '\0';
        web_send(web_connfd, buffer);
    }
}

void report_noreturn(int level, char *fmt, ...)
{
    if (!verbfile)
        init_files(stdout, stdout);

    char buffer[BUF_SIZE];
    if (level <= verblevel) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(verbfile, fmt, ap);
        fflush(verbfile);
        va_end(ap);

        if (logfile) {
            va_start(ap, fmt);
            vfprintf(logfile, fmt, ap);
            fflush(logfile);
            va_end(ap);
        }
        va_start(ap, fmt);
        vsnprintf(buffer, BUF_SIZE, fmt, ap);
        va_end(ap);
    }

    if (web_connfd)
        web_send(web_connfd, buffer);
}

/* Functions denoting failures */

/* Need to be able to print without using malloc */
static void fail_fun(char *format, char *msg)
{
    snprintf(fail_buf, sizeof(fail_buf), format, msg);
    /* Tack on return */
    fail_buf[strlen(fail_buf)] = '\n';
    /* Use write to avoid any buffering issues */
    ret = write(STDOUT_FILENO, fail_buf, strlen(fail_buf) + 1);

    if (logfile) {
        /* Don't know file descriptor for logfile */
        fputs(fail_buf, logfile);
    }

    if (fatal_fun)
        fatal_fun();

    if (logfile)
        fclose(logfile);

    exit(1);
}

/* Maximum number of megabytes that application can use (0 = unlimited) */
static int mblimit = 0;

/* Keeping track of memory allocation */
static size_t allocate_cnt = 0;
static size_t allocate_bytes = 0;
static size_t free_cnt = 0;
static size_t free_bytes = 0;

/* Counters giving peak memory usage */
static size_t peak_bytes = 0;
static size_t last_peak_bytes = 0;
static size_t current_bytes = 0;

static void check_exceed(size_t new_bytes)
{
    size_t limit_bytes = (size_t) mblimit << 20;
    size_t request_bytes = new_bytes + current_bytes;
    if (mblimit > 0 && request_bytes > limit_bytes) {
        report_event(MSG_FATAL,
                     "Exceeded memory limit of %u megabytes with %lu bytes",
                     mblimit, request_bytes);
    }
}

/* Call malloc & exit if fails */
void *malloc_or_fail(size_t bytes, char *fun_name)
{
    check_exceed(bytes);
    void *p = malloc(bytes);
    if (!p) {
        fail_fun("Malloc returned NULL in %s", fun_name);
        return NULL;
    }

    allocate_cnt++;
    allocate_bytes += bytes;
    current_bytes += bytes;
    peak_bytes = MAX(peak_bytes, current_bytes);
    last_peak_bytes = MAX(last_peak_bytes, current_bytes);

    return p;
}

/* Call calloc returns NULL & exit if fails */
void *calloc_or_fail(size_t cnt, size_t bytes, char *fun_name)
{
    check_exceed(cnt * bytes);
    void *p = calloc(cnt, bytes);
    if (!p) {
        fail_fun("Calloc returned NULL in %s", fun_name);
        return NULL;
    }

    allocate_cnt++;
    allocate_bytes += cnt * bytes;
    current_bytes += cnt * bytes;
    peak_bytes = MAX(peak_bytes, current_bytes);
    last_peak_bytes = MAX(last_peak_bytes, current_bytes);

    return p;
}

char *strsave_or_fail(char *s, char *fun_name)
{
    if (!s)
        return NULL;

    size_t len = strlen(s);
    check_exceed(len + 1);
    char *ss = malloc(len + 1);
    if (!ss)
        fail_fun("strsave failed in %s", fun_name);

    allocate_cnt++;
    allocate_bytes += len + 1;
    current_bytes += len + 1;
    peak_bytes = MAX(peak_bytes, current_bytes);
    last_peak_bytes = MAX(last_peak_bytes, current_bytes);

    return strncpy(ss, s, len + 1);
}

/* Free block, as from malloc, realloc, or strsave */
void free_block(void *b, size_t bytes)
{
    if (!b)
        report_event(MSG_ERROR, "Attempting to free null block");
    free(b);

    free_cnt++;
    free_bytes += bytes;
    current_bytes -= bytes;
}

/* Free array, as from calloc */
void free_array(void *b, size_t cnt, size_t bytes)
{
    if (!b)
        report_event(MSG_ERROR, "Attempting to free null block");
    free(b);

    free_cnt++;
    free_bytes += cnt * bytes;
    current_bytes -= cnt * bytes;
}

/* Free string saved by strsave_or_fail */
void free_string(char *s)
{
    if (!s)
        report_event(MSG_ERROR, "Attempting to free null block");
    free_block((void *) s, strlen(s) + 1);
}

/* Initialization of timers */
void init_time(double *timep)
{
    (void) delta_time(timep);
}

double delta_time(double *timep)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double current_time = tv.tv_sec + 1.0E-6 * tv.tv_usec;
    double delta = current_time - *timep;
    *timep = current_time;
    return delta;
}

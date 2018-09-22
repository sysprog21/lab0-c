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

#define MAX(a, b) ((a) < (b) ? (b) : (a))

FILE *errfile = NULL;
FILE *verbfile = NULL;
FILE *logfile = NULL;

int verblevel = 0;
void init_files(FILE *efile, FILE *vfile)
{
    errfile = efile;
    verbfile = vfile;
}

static char fail_buf[1024] = "FATAL Error.  Exiting\n";

volatile int rval = 0;

/* Default fatal function */
void default_fatal_fun()
{
    /*
    sprintf(fail_buf, "FATAL.  Memory: allocated = %.3f GB, resident = %.3f
    GB\n",
           gigabytes(current_bytes), gigabytes(resident_bytes()));
    */
    rval = write(STDOUT_FILENO, fail_buf, strlen(fail_buf) + 1);
    if (logfile)
        fputs(fail_buf, logfile);
}

/* Optional function to call when fatal error encountered */
void (*fatal_fun)() = default_fatal_fun;

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
    char *msg_name = msg == MSG_WARN
                         ? "WARNING"
                         : msg == MSG_ERROR ? "ERROR" : "FATAL ERROR";
    int level = msg == MSG_WARN ? 2 : msg == MSG_ERROR ? 1 : 0;
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


void report(int level, char *fmt, ...)
{
    va_list ap;
    if (!verbfile)
        init_files(stdout, stdout);
    if (level <= verblevel) {
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
    }
}

void report_noreturn(int level, char *fmt, ...)
{
    va_list ap;
    if (!verbfile)
        init_files(stdout, stdout);
    if (level <= verblevel) {
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
    }
}

void safe_report(int level, char *msg)
{
    if (level > verblevel)
        return;
    if (!errfile)
        init_files(stdout, stdout);
    fputs(msg, errfile);
    if (logfile) {
        fputs(msg, logfile);
    }
}



/* Functions denoting failures */

/* General failure */

/* Need to be able to print without using malloc */
void fail_fun(char *format, char *msg)
{
    sprintf(fail_buf, format, msg);
    /* Tack on return */
    fail_buf[strlen(fail_buf)] = '\n';
    /* Use write to avoid any buffering issues */
    rval = write(STDOUT_FILENO, fail_buf, strlen(fail_buf) + 1);
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
int mblimit = 0;
/* Maximum number of seconds that application can use.  (0 = unlimited)  */
int timelimit = 0;

/* Keeping track of memory allocation */
static size_t allocate_cnt = 0;
static size_t allocate_bytes = 0;
static size_t free_cnt = 0;
static size_t free_bytes = 0;
/* These are externally visible */
size_t peak_bytes = 0;
size_t last_peak_bytes = 0;
size_t current_bytes = 0;

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

/* Call realloc returns NULL & exit if fails.
   Require explicit indication of current allocation */
void *realloc_or_fail(void *old,
                      size_t old_bytes,
                      size_t new_bytes,
                      char *fun_name)
{
    if (new_bytes > old_bytes)
        check_exceed(new_bytes - old_bytes);
    void *p = realloc(old, new_bytes);
    if (!p) {
        fail_fun("Realloc returned NULL in %s", fun_name);
        return NULL;
    }
    allocate_cnt++;
    allocate_bytes += new_bytes;
    current_bytes += (new_bytes - old_bytes);
    peak_bytes = MAX(peak_bytes, current_bytes);
    last_peak_bytes = MAX(last_peak_bytes, current_bytes);
    free_cnt++;
    free_bytes += old_bytes;
    return p;
}

char *strsave_or_fail(char *s, char *fun_name)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    check_exceed(len + 1);
    char *ss = malloc(len + 1);
    if (!ss) {
        fail_fun("strsave failed in %s", fun_name);
    }
    allocate_cnt++;
    allocate_bytes += len + 1;
    current_bytes += len + 1;
    peak_bytes = MAX(peak_bytes, current_bytes);
    last_peak_bytes = MAX(last_peak_bytes, current_bytes);

    return strcpy(ss, s);
}

/* Free block, as from malloc, realloc, or strsave */
void free_block(void *b, size_t bytes)
{
    if (b == NULL) {
        report_event(MSG_ERROR, "Attempting to free null block");
    }
    free(b);
    free_cnt++;
    free_bytes += bytes;
    current_bytes -= bytes;
}

/* Free array, as from calloc */
void free_array(void *b, size_t cnt, size_t bytes)
{
    if (b == NULL) {
        report_event(MSG_ERROR, "Attempting to free null block");
    }
    free(b);
    free_cnt++;
    free_bytes += cnt * bytes;
    current_bytes -= cnt * bytes;
}

/* Free string saved by strsave_or_fail */
void free_string(char *s)
{
    if (s == NULL) {
        report_event(MSG_ERROR, "Attempting to free null block");
    }
    free_block((void *) s, strlen(s) + 1);
}


/* Report current allocation status */
void mem_status(FILE *fp)
{
    fprintf(fp,
            "Allocated cnt/bytes: %lu/%lu.  Freed cnt/bytes: %lu/%lu.\n"
            "  Peak bytes %lu, Last peak bytes %ld, Current bytes %ld\n",
            (long unsigned) allocate_cnt, (long unsigned) allocate_bytes,
            (long unsigned) free_cnt, (long unsigned) free_bytes,
            (long unsigned) peak_bytes, (long unsigned) last_peak_bytes,
            (long unsigned) current_bytes);
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

/* Number of bytes resident in physical memory */
size_t resident_bytes()
{
    struct rusage r;
    size_t mem = 0;
    int code = getrusage(RUSAGE_SELF, &r);
    if (code < 0) {
        report_event(MSG_ERROR, "Call to getrusage failed");
    } else {
        mem = r.ru_maxrss * 1024;
    }
    return mem;
}

double gigabytes(size_t n)
{
    return (double) n / (1UL << 30);
}

void reset_peak_bytes()
{
    last_peak_bytes = current_bytes;
}

#if 0
static char timeout_buf[256];

void sigalrmhandler(int sig) {
    safe_report(true, timeout_buf);
}


void change_timeout(int oldval) {
    sprintf(timeout_buf, "Program timed out after %d seconds\n", timelimit);
    /* alarm function will correctly cancel existing alarms */
    signal(SIGALRM, sigalrmhandler);
    alarm(timelimit);
}

/* Handler for SIGTERM signals */
void sigterm_handler(int sig) {
    safe_report(true, "SIGTERM signal received");
}
#endif

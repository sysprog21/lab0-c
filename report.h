#ifndef LAB0_REPORT_H
#define LAB0_REPORT_H

#include <stdarg.h>
#include <stdbool.h>

/* Ways to report interesting behavior and errors */

/* Things to report */
typedef enum { MSG_WARN, MSG_ERROR, MSG_FATAL, N_MSG } message_t;

/* Buffer sizes */
#define MAX_CHAR 512

bool set_logfile(char *file_name);

extern int verblevel;
void set_verblevel(int level);

/* Error messages */
void report_event(message_t msg, char *fmt, ...);

/* Report useful information */
void report(int verblevel, char *fmt, ...);

/* Like report, but without return character */
void report_noreturn(int verblevel, char *fmt, ...);

/* Attempt to call malloc.  Fail when returns NULL */
void *malloc_or_fail(size_t bytes, char *fun_name);

/* Attempt to call calloc.  Fail when returns NULL */
void *calloc_or_fail(size_t cnt, size_t bytes, char *fun_name);

/* Attempt to save string.  Fail when malloc returns NULL */
char *strsave_or_fail(char *s, char *fun_name);

/* Free block, as from malloc, or strsave */
void free_block(void *b, size_t len);

/* Free array, as from calloc */
void free_array(void *b, size_t cnt, size_t bytes);

/* Free string saved by strsave_or_fail */
void free_string(char *s);

/* Time counted as fp number in seconds */
void init_time(double *timep);

/* Compute time since last call with this timer and reset timer */
double delta_time(double *timep);

#endif /* LAB0_REPORT_H */

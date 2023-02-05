#ifndef LAB0_CONSOLE_H
#define LAB0_CONSOLE_H

#include <stdbool.h>
#include <sys/select.h>

#include "linenoise.h"

#define HISTORY_FILE ".cmd_history"

/* Implementation of simple command-line interface */

/* Simulation flag of console option */
extern int simulation;

/* Each command defined in terms of a function */
typedef bool (*cmd_func_t)(int argc, char *argv[]);

/* Information about each command */

/* Organized as linked list in alphabetical order */
typedef struct __cmd_element {
    char *name;
    cmd_func_t operation;
    char *summary;
    char *param;
    struct __cmd_element *next;
} cmd_element_t;

/* Optionally supply function that gets invoked when parameter changes */
typedef void (*setter_func_t)(int oldval);

/* Integer-valued parameters */
typedef struct __param_element {
    char *name;
    int *valp;
    char *summary;
    /* Function that gets called whenever parameter changes */
    setter_func_t setter;
    struct __param_element *next;
} param_element_t;

/* Initialize interpreter */
void init_cmd();

/* Add a new command */
void add_cmd(char *name, cmd_func_t operation, char *summary, char *parameter);
#define ADD_COMMAND(cmd, msg, param) add_cmd(#cmd, do_##cmd, msg, param)

/* Add a new parameter */
void add_param(char *name, int *valp, char *summary, setter_func_t setter);

/* Extract integer from text and store at loc */
bool get_int(char *vname, int *loc);

/* Add function to be executed as part of program exit */
void add_quit_helper(cmd_func_t qf);

/* Turn echoing on/off */
void set_echo(bool on);

/* Complete command interpretation */

/* Return true if no errors occurred */
bool finish_cmd();

/* Run command loop.  Non-null infile_name implies read commands from that file
 */
bool run_console(char *infile_name);

/* Callback function to complete command by linenoise */
void completion(const char *buf, line_completions_t *lc);

#endif /* LAB0_CONSOLE_H */

/* Implementation of simple command-line interface */

/* Each command defined in terms of a function */
typedef bool (*cmd_function)(int argc, char *argv[]);

/* Information about each command */
/* Organized as linked list in alphabetical order */
typedef struct CELE cmd_ele, *cmd_ptr;
struct CELE {
    char *name;
    cmd_function operation;
    char *documentation;
    cmd_ptr next;
};

/* Optionally supply function that gets invoked when parameter changes */
typedef void (*setter_function)(int oldval);

/* Integer-valued parameters */
typedef struct PELE param_ele, *param_ptr;
struct PELE {
    char *name;
    int *valp;
    char *documentation;
    /* Function that gets called whenever parameter changes */
    setter_function setter;
    param_ptr next;
};

/* Initialize interpreter */
void init_cmd();

/* Add a new command */
void add_cmd(char *name, cmd_function operation, char *documentation);

/* Add a new parameter */
void add_param(char *name,
               int *valp,
               char *doccumentation,
               setter_function setter);

/* Execute a command from a command line */
bool interpret_cmd(char *cmdline);

/* Execute a sequence of commands read from a file */
bool interpret_file(FILE *fp);

/* Extract integer from text and store at loc */
bool get_int(char *vname, int *loc);

/* Add function to be executed as part of program exit */
void add_quit_helper(cmd_function qf);

/* Set prompt */
void set_prompt(char *prompt);

/* Turn echoing on/off */
void set_echo(bool on);

/*
  Some console commands require network activity to complete.
  Program maintains a flag indicating whether console is ready to execute a new
  command (unblocked) or it must wait for pending network activity (blocked).

  The following calls set/clear that flag
*/

void block_console();
void unblock_console();

/* Start command intrepretation.
   If infile_name is NULL, then read commands from stdin
   Otherwise, use infile as command source
*/
bool start_cmd(char *infile_name);


/* Is it time to quit the command loop? */
bool cmd_done();

/* Complete command interpretation */
/* Return true if no errors occurred */
bool finish_cmd();

/*
   Handle command processing in program that uses select as main
   control loop.  Like select, but (if console unblocked) it checks
   whether command input either present in internal buffer or readable
   from command input.  If so, that command is executed.  Same return
   as select.  Command input file removed from readfds

   nfds should be set to the maximum file descriptor for network sockets.
   If nfds == 0, this indicates that there is no pending network activity
*/

int cmd_select(int nfds,
               fd_set *readfds,
               fd_set *writefds,
               fd_set *exceptfds,
               struct timeval *timeout);

/* Run command loop.  Non-null infile_name implies read commands from that file
 */
bool run_console(char *infile_name);

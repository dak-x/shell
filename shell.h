/* ==================================== */

/* Structure to hold info about a process name and args*/
struct Proc {
  char *proc;
};

struct Proc from_str(char *);

/* Definitions for HISTORY list
  Since we need to traverse the History in reverse chronology as well
  a doubly-linked-list seems approriete
*/

// Linked List Node for history of all commands executed
struct HisNode {
  struct HisNode *Next, *Prev;
  struct Proc cmd;
  int pid;
};

int HISSIZE = 0;
void add_his(struct Proc, int);
void free_his();
/* ==================================== */

/* Definitions for list of background processes
  This is a singly linked list as we would only need to traverse once
*/

// Linked List Node for background process pool
struct BackGroundPool {
  struct BackGroundPool *Next;
  struct Proc cmd;
  int pid;
  int terminated;
};

void add_bgproc(struct Proc, int);
void clean_bgpool();

/* ==================================== */
// Interactive mode for the shell
// Display Utils

void prompt();
void invalid_cmd(char *);
void exec_cmd();

// Add foreground and background support.

void run_fg(struct Proc);
void run_bg(struct Proc);

/* ==================================== */
// User Commands

void print_hisn(int);
void exec_hisn(int);
void pid();
void pid_all();
void pid_current();

// String Manipulation and Utils

char *trim(char *);
int is_empty(char *);
int is_integer(char *);
int is_bg(char *);
void exit_routine();

/* Adding support for I/O redirection and spawning piped processes */
//
// Ideally a Piped Command can enclose a simple non-piped command but I
// was afraid to modify existing code without a working solution.
// Since now I get expected shell functionality. Merging both non-piped and
// piped commands can be the next exercise for me.
//
/* Hold info of all processes to spawn */
struct PipeList {
  struct PipeList *Next;
  char *cmd;
  int pid;
};

int is_piped(char *);
struct PipeList *decode_pipe(char *);
void exec_pipe(char *);
void free_pipelist(struct PipeList *);

// Get redirection tokens

char *get_in_redir(char *);
char *get_out_redir(char *);

// sig-handler which waits the processes
void handle_sigchild(int);

/* ==================================== */

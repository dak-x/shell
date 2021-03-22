/* main function is global entry point
 * Shell with IO Redirect and Pipe capabilities
 */
#include "shell.h"
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

// Some globals
struct HisNode *HISHEAD = NULL, *HISTAIL = NULL;
struct BackGroundPool *BGPOOL = NULL, *BGTAIL = NULL;

char* ROOT;
int LENROOT;
char* PROCNAME;

/* SOME CONSTANTS */
#define MAXARGS 128
#define MAXBUFF 1024
#define PIDINV -1
#define delim " "
/* ==================================== */

int main(int argc, char* argv[])
{

    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, handle_sigchild);

    // INIT PHASE:
    // Set process ROOT to be containing folder
    PROCNAME = argv[0];
    ROOT = getcwd(NULL, 0);
    LENROOT = strlen(ROOT);
    chroot(ROOT);
    char buff[MAXBUFF];
    buff[0] = '\0';
    int got_eof = 0;
    int x;
    // INIT OVER.

    while (1) {
        // clean_bgpool();
        prompt();
        // Handle the ctrl+D or EOF character
        if (fgets(buff, sizeof(buff), stdin) == NULL) {
            if (got_eof == 0 && BGPOOL != NULL) {
                printf("\nThere are still jobs active.\n\n");
                pid_current();
                printf("A second attempt to exit will terminate all of them.\n");
                got_eof = 1;
                continue;
            } else {
                // Terminate the jobs and the shell as well.
                exit_routine();
            }
        }

        char* inp = strdup(trim(buff));
        if (is_empty(inp)) {
            continue;
        }

        exec_cmd(inp);
        got_eof = 0;
    }
}

/* Function Definations */
/* ==================================== */

// Get a Proc struct from an string.
struct Proc from_str(char* cmd)
{
    char* cpy = strdup(cmd);
    struct Proc p;
    p.proc = cpy;
    return p;
}

// Add a process to the history.
void add_his(struct Proc p, int pid)
{
    struct HisNode* temp = (struct HisNode*)malloc(sizeof(struct HisNode));
    struct Proc p_cpy;
    p_cpy.proc = strdup(p.proc);

    temp->Next = temp->Prev = NULL;
    temp->pid = pid;
    temp->cmd = p_cpy;

    HISSIZE++;
    if (HISTAIL == NULL) {
        HISTAIL = HISHEAD = temp;
    } else {
        HISTAIL->Next = temp;
        temp->Prev = HISTAIL;
        HISTAIL = temp;
    }
}

// Free the currently allocated linked list
void free_his()
{
    while (HISHEAD != NULL) {
        struct HisNode* temp = HISHEAD->Next;
        free(HISHEAD->cmd.proc);
        free(HISHEAD);
        HISHEAD = temp;
    }
}
/* ==================================== */
// User Commands

// Print the last n commands from history
void print_hisn(int n)
{
    int N = n < HISSIZE ? n : HISSIZE;
    struct HisNode* curr = HISTAIL;
    for (int i = 1; i <= N; i++) {
        printf("%d. %s\n", i, curr->cmd.proc);
        curr = curr->Prev;
    }
}

// Execute the nth command from last
void exec_hisn(int n)
{
    if (n < 1 || n > HISSIZE) {
        printf("n: %d is either too large or too small", n);
    }
    n--;
    struct HisNode* temp = HISTAIL;
    while (n--) {
        temp = temp->Prev;
    }

    exec_cmd(temp->cmd.proc);
}

// prints the process id of your shell program
void pid() { printf("command name: %s process id: %d\n", PROCNAME, getpid()); }

// print pid of all processes spawned from this shell
void pid_all()
{
    printf("List of all processes spawned from this shell:\n");
    int N = HISSIZE;
    struct HisNode* curr = HISHEAD;
    for (int i = 1; i <= HISSIZE; i++) {
        printf("command name: %-30s pid: %d\n", curr->cmd.proc, curr->pid);
        curr = curr->Next;
    }
}

// print pid of all currently running spawned processes
void pid_current()
{
    clean_bgpool();
    printf("List of currently executing processes spawned from this shell:\n");
    struct BackGroundPool* curr = BGPOOL;
    while (curr != NULL) {
        if (curr->pid != PIDINV)
            printf("command name: %-30s pid: %d\n", curr->cmd.proc, curr->pid);
        curr = curr->Next;
    }
}
/* Display the prompt user@system_name:curr_dir> */
// ? Maybe Buffer it at the start of the program  */
// * Ideally no memory leaks here. Free everything allocated before exiting.
void prompt()
{
    char* user = getpwuid(geteuid())->pw_name;
    struct utsname* uts = (struct utsname*)malloc(sizeof(struct utsname));
    uname(uts);
    char* sys_name = uts->nodename;
    // Get the current working directory and also replace the root with '~'
    char* cwd = getcwd(NULL, 0);
    char* cwd_sub = cwd;
    if (strncmp(cwd, ROOT, LENROOT) == 0) {
        cwd_sub = &cwd[LENROOT - 1];
        cwd_sub[0] = '~';
    }
    // Display the prompt
    printf("<%s@%s:%s> ", user, sys_name, cwd_sub);
    free(cwd);
    free(uts);
}

// String Manipulation Functions
/* ==================================== */

// Trim the ends of the string of whitespaces and newlines
// The caller must free the `str` passed and not the returned string
char* trim(char* str)
{
    int n = strlen(str);

    // Trim the prefix
    int i = 0;
    char* temp = str;
    while (i < n && (temp[0] == ' ' || temp[0] == '\t' || temp[0] == '\r' || temp[0] == '\n')) {
        // temp[i] = '\0';
        temp++;
        i++;
    }
    i = strlen(temp) - 1;
    while (i > 0 && (temp[i] == ' ' || temp[i] == '\n' || temp[i] == '\r' || temp[i] == '\t')) {
        temp[i] = '\0';
        i--;
    }
    // Replace '/t' with ' ' for splitting
    while (i >= 0) {
        if (temp[i] == '\t' || temp[i] == '\r')
            temp[i] = ' ';
        i--;
    }

    return temp;
}

// Check if the given string is empty
int is_empty(char* str)
{
    if (str == NULL || strlen(str) == 0)
        return 1;
    return 0;
}

// Return the Integer Value of scanned string if an integer, else returns -1
int is_integer(char* st)
{
    int n = strlen(st), i = 0, val = 0;
    while (i < n) {
        if (!isdigit(st[i]))
            return -1;
        val = val * 10 + (st[i] - '0');
        i++;
    }
    return val;
}

// Returns whether the cmd has a '&' appended to it. Note: Pass a trimmed
// string
int is_bg(char* cmd) { return cmd[strlen(cmd) - 1] == '&'; }
/* ==================================== */

// Spawn Process utils

// Add the given proc to backgrnd process pool.
void add_bgproc(struct Proc p, int pid)
{

    signal(SIGCHLD, SIG_IGN);

    struct BackGroundPool* temp = (struct BackGroundPool*)malloc(sizeof(struct BackGroundPool));

    char* cmd = strdup(p.proc);
    struct Proc p_cpy;
    p_cpy.proc = cmd;

    temp->cmd = p_cpy;
    temp->pid = pid;
    temp->terminated = 0;
    temp->Next = NULL;

    if (BGPOOL == NULL) {
        BGPOOL = BGTAIL = temp;
    } else {
        BGTAIL->Next = temp;
        BGTAIL = temp;
    }

    signal(SIGCHLD, handle_sigchild);
}

// Clean All processes from the pool
void clean_bgpool()
{

    // check for the head
    int stat;
    while (BGPOOL != NULL && BGPOOL->pid == waitpid(BGPOOL->pid, &stat, WNOHANG)) {
        struct BackGroundPool* temp = BGPOOL;
        printf("Job '%s' has ended\n", temp->cmd.proc);
        BGPOOL = BGPOOL->Next;
        free(temp);
        temp = NULL;
    }
    if (BGPOOL == NULL)
        return;

    struct BackGroundPool* curr = BGPOOL;
    while (curr->Next != NULL) {
        // Proc has terminated, remove from the list
        if (curr->Next->pid == waitpid(curr->Next->pid, &stat, WNOHANG)) {
            printf("Job '%s' has ended\n", curr->Next->cmd.proc);
            struct BackGroundPool* temp = curr->Next;
            curr->Next = temp->Next;
            free(temp->cmd.proc);
            free(temp);
        }
        curr = curr->Next;
    }
}

// Run a foreground process
void run_fg(struct Proc p)
{

    // Spawn the child
    int PID = fork();

    if (PID < (pid_t)0) {
        printf("Cannot Spawn more processes. Exiting...");
        exit_routine();
    } else if (PID == (pid_t)0) {

        char* cm = p.proc;
        char* inp = get_in_redir(cm);
        char* out = get_out_redir(cm);

        // syntax error: No Token found after '<' or '>'
        if (inp != NULL && strlen(inp) == 0) {
            printf("No String Token found after last occurence of '<' \n");
            _exit(EXIT_FAILURE);
        }
        if (out != NULL && strlen(out) == 0) {
            printf("No String Token found after last occurence of '>' \n");
            _exit(EXIT_FAILURE);
        }

        // Set the In file descriptor
        if (inp != NULL) {
            FILE* f_in = fopen(inp, "r");
            if (f_in == NULL) {
                perror(inp);
                _exit(EXIT_FAILURE);
            }
            dup2(fileno(f_in), STDIN_FILENO);
        }
        // Set Out file descriptor
        if (out != NULL) {
            FILE* f_out = fopen(out, "w");
            if (f_out == NULL) {
                perror(out);
                _exit(EXIT_FAILURE);
            }
            dup2(fileno(f_out), STDOUT_FILENO);
        }

        char* args[MAXARGS];
        int i = 0;
        char* arg = strtok(cm, delim);
        while (i < MAXARGS && arg != NULL) {
            args[i] = arg;
            arg = strtok(NULL, delim);
            i++;
        }
        args[i] = NULL;
        char* cmd = args[0];

        int err = execvp(cmd, args);
        perror(cmd);
        _exit(err);

    } else {
        // Add process to history
        add_his(p, PID);
        int wstatus;
        waitpid(PID, &wstatus, 0);
    }
}

// Run a background process
void run_bg(struct Proc p)
{

    // Spawn the child
    int PID = fork();

    if (PID < (pid_t)0) {
        printf("Cannot Spawn more processes. Exiting...");
        exit_routine();
    } else if (PID == (pid_t)0) {
        // Change process grp and also set to background
        setpgid(0, 0);
        // tcsetpgrp(STDIN_FILENO, pgrpid);
        char* cm = p.proc;
        cm[strlen(cm) - 1] = '\0'; // remove the '&'

        char* inp = get_in_redir(cm);
        char* out = get_out_redir(cm);

        // syntax error: No Token found after '<' or '>'
        if (inp != NULL && strlen(inp) == 0) {
            printf("No String Token found after last occurence of '<' \n");
            _exit(EXIT_FAILURE);
        }
        if (out != NULL && strlen(out) == 0) {
            printf("No String Token found after last occurence of '>' \n");
            _exit(EXIT_FAILURE);
        }

        // Set the In file descriptor
        if (inp != NULL) {
            FILE* f_in = fopen(inp, "r");
            if (f_in == NULL) {
                perror(inp);
                _exit(EXIT_FAILURE);
            }
            dup2(fileno(f_in), STDIN_FILENO);
        }
        // Set Out file descriptor
        if (out != NULL) {
            FILE* f_out = fopen(out, "w");
            if (f_out == NULL) {
                perror(out);
                _exit(EXIT_FAILURE);
            }
            dup2(fileno(f_out), STDOUT_FILENO);
        }

        // get the args
        char* args[MAXARGS];
        int i = 0;
        char* arg = strtok(cm, delim);
        while (i < MAXARGS && arg != NULL) {
            args[i] = arg;
            arg = strtok(NULL, delim);
            i++;
        }
        args[i] = NULL;

        char* cmd = args[0];
        int err = execvp(cmd, args);
        // Print the error
        perror(cmd);
        _exit(err);

    } else { // No wait
        // kill(SIGTTIN,PID);
        setpgid(PID, PID);
        add_his(p, PID);
        add_bgproc(p, PID);
    }
}

// Clean up
void exit_routine()
{
    // Kill All processes in the process pool
    // block all signals for now.

    free_his();

    struct BackGroundPool* curr = BGPOOL;
    while (curr != NULL) {
        kill(curr->pid, SIGKILL);
        curr = curr->Next;
    }

    while (BGPOOL != NULL) {
        waitpid(BGPOOL->pid, NULL, 0);
        struct BackGroundPool* temp = BGPOOL;
        BGPOOL = BGPOOL->Next;
        free(temp);
        temp = NULL;
    }
    printf("\nHave a nice day Bye!!\n");
    exit(EXIT_SUCCESS);
}

// Couldnot find command in PATH
void invalid_cmd(char* cmd) { printf("%s: Command Not Found\n", cmd); }

void exec_sys_cmd(struct Proc p)
{
    if (is_bg(p.proc)) {
        run_bg(p);
    } else {
        run_fg(p);
    }
}

// Decode and Execute
void exec_cmd(char* cmd)
{

    char* cmd_temp = strdup(cmd);
    struct Proc p;
    p.proc = cmd;

    // Decode for USER COMMANDS
    char* first_cmd = strtok(cmd_temp, " ");
    int temp = -1;
    // change dir
    if (strcmp(first_cmd, "cd") == 0) {
        char* dir = strtok(NULL, "");
        if (chdir(dir) == -1) {
            perror(dir);
        } else {
            add_his(p, getpid());
        }
    }
    // pid user commands
    else if (strcmp(first_cmd, "pid") == 0) {
        char* sbcmd = strtok(NULL, " ");
        if (sbcmd == NULL) {
            pid();
        } else {
            char* third_cmd = strtok(NULL, " ");

            if (is_empty(third_cmd)) {
                // pid current
                if (strcmp(sbcmd, "current") == 0) {
                    pid_current();
                    add_his(p, getpid());
                }
                // pid all
                else if (strcmp(sbcmd, "all") == 0) {
                    pid_all();
                    add_his(p, getpid());
                }

                else {
                    exec_sys_cmd(p);
                }
            } else {
                exec_sys_cmd(p);
            }
        }

    } else if (strcmp(first_cmd, "STOP") == 0) {
        if (!is_empty(strtok(NULL, ""))) {
            exec_sys_cmd(p);
        } else {
            exit_routine();
        }
    }

    // HIST and !HIST
    else if (sscanf(first_cmd, "HIST%d", &temp) == 1 && temp != -1) {
        if (!is_empty(strtok(NULL, "")))
            exec_sys_cmd(p);
        else {
            print_hisn(temp);
        }
    } else if (sscanf(first_cmd, "!HIST%d", &temp) == 1 && temp != -1) {
        if (!is_empty(strtok(NULL, "")))
            exec_sys_cmd(p);
        else {
            exec_hisn(temp);
        }
    }

    // system cmd
    else {
        if (is_piped(p.proc)) {
            // execute a piped command.
            exec_pipe(p.proc);
        } else {
            exec_sys_cmd(p);
        }
    }
    // Free some memory leaks.
    free(cmd_temp);
    free(cmd);
}

void handle_sigchild(int signum) { clean_bgpool(); }

// is_sep(c) => NOT isalnum(c) {but better}.
int is_sep(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '<' || c == '|';
}

// Get the filename for input redirection and replace with filename with
// whitespace in the orig string. Returns:
// ------
// NULL if '<' not found ,
// "" is there is not token after '<',
// String token succeeding the '<'. User must free this allocated string
char* get_in_redir(char* orig)
{
    // First occurence of '<'
    char* x = strchr(orig, '<');
    // '<' not found
    if (x == NULL)
        return NULL;

    // store positioon of '<' char
    char* less_pos = x;
    x++;
    while (*x != '\0' && isspace(*x))
        x++;

    if (*x == '\0' || *x == '>' || *x == '|' || *x == '\r' || *x == '\n')
        return "";

    // '<' is found
    // Move to the next white space char and then tokenize between these ends.
    char* x_cpy = x;
    while (*x != '\0' && !is_sep(*x)) {
        x++;
    }

    char* s = strndup(x_cpy, x - x_cpy);
    // blank out the string from orig
    while (x_cpy < x) {
        *x_cpy = ' ';
        x_cpy++;
    }
    *less_pos = ' ';
    return s;
}

// Get the filename for output redirection and replace with filename with spaces
// in the orig string. Returns:
// NULL if '>' not found ,
// "" is there is not token after '>',
// String token succeeding the '>'. User must free this allocated string.
char* get_out_redir(char* orig)
{
    // First occurence of '>'
    char* x = strchr(orig, '>');
    // '<' not found
    if (x == NULL)
        return NULL;

    // store position of '>' char
    char* less_pos = x;
    x++;
    while (*x != '\0' && isspace(*x))
        x++;

    if (*x == '\0' || *x == '>' || *x == '|' || *x == '\r' || *x == '\n')
        return "";

    // '>' is found
    // Move to the next white space char and then tokenize between these ends.
    char* x_cpy = x;
    while (*x != '\0' && !is_sep(*x)) {
        x++;
    }
    char* s = strndup(x_cpy, x - x_cpy);
    // blank out the string from orig
    while (x_cpy < x) {
        *x_cpy = ' ';
        x_cpy++;
    }
    *less_pos = ' ';
    return s;
}

// Checks whether the command has pipes
int is_piped(char* cmd) { return !(strchr(cmd, '|') == NULL); }

// Make a piped list from given string
// Returns NULL in case of errors.
// caller must free cmd.
struct PipeList* decode_pipe(char* cmd)
{

    char* cm = strtok(strdup(cmd), "|");
    struct PipeList* head = NULL;
    struct PipeList* curr = head;

    while (cm != NULL) {
        if (is_empty(trim(cm))) {
            free_pipelist(head);
            printf("Expected Command after '|'. \n");
            return NULL;
        }
        struct PipeList* temp = (struct PipeList*)malloc(sizeof(struct PipeList));
        temp->Next = NULL;
        temp->pid = -1;
        temp->cmd = strdup(trim(cm));

        if (head == NULL)
            head = curr = temp;
        else {
            curr->Next = temp;
            curr = temp;
        }

        cm = strtok(NULL, "|");
    }

    free(cm);
    return head;
}
// Execute a piped command.
void exec_pipe(char* cmd)
{
    struct Proc p;
    p.proc = cmd;

    struct PipeList* start = decode_pipe(cmd);
    if (start == NULL) {
        return;
    }
    // Init the structure for the pipe.
    int pipefd[2];
    pipefd[0] = -1;
    pipefd[1] = -1;
    int r_fd = -1;

    struct PipeList* temp = start;
    int flg = -1; // Store pid of first process only

    // Spawn all the piped processes in parallel.
    while (temp != NULL) {
        // More pipes reqd
        if (temp->Next != NULL) {
            if (pipe(pipefd) == -1) {
                printf("Cannot Open New Pipes. This is very strange. Exiting...");
                exit_routine();
            }

        } else {
            pipefd[0] = -1;
            pipefd[1] = -1;
        }
        // create a pipe

        // I have pipes now appropritely assign them.
        char* cm = temp->cmd;
        // Spawn the child.
        int PID = fork();

        if (PID < (pid_t)0) {
            printf("Cannot Spawn more processes. Exiting...");
            exit_routine();
        } else if (PID == (pid_t)0) {
            printf("Exec-ed: %s", cm);
            char* inp = get_in_redir(cm);
            char* out = get_out_redir(cm);

            // syntax error: No Token found after '<' or '>'
            if (inp != NULL && strlen(inp) == 0) {
                printf("No String Token found after last occurence of '<' \n");
                _exit(EXIT_FAILURE);
            }
            if (out != NULL && strlen(out) == 0) {
                printf("No String Token found after last occurence of '>' \n");
                _exit(EXIT_FAILURE);
            }

            // Set the In file descriptor
            if (inp != NULL) {
                FILE* f_in = fopen(inp, "r");
                if (f_in == NULL) {
                    perror(inp);
                    _exit(EXIT_FAILURE);
                }
                dup2(fileno(f_in), STDIN_FILENO);
            }
            // Set the Out file descriptor
            if (out != NULL) {
                FILE* f_out = fopen(out, "w");
                if (f_out == NULL) {
                    perror(out);
                    _exit(EXIT_FAILURE);
                }
                dup2(fileno(f_out), STDOUT_FILENO);
            }
            // ================================================
            // SET THE PIPES. PIPES OVERRIDE THE IO-REDIRECTIONS

            // Also Close other file descriptors as well
            if (pipefd[0] == -1)
                close(pipefd[0]);

            // Set the Read fd
            if (r_fd != -1) {
                dup2(r_fd, STDIN_FILENO);
            }
            // Set the Write fd
            if (pipefd[1] != -1) {
                dup2(pipefd[1], STDOUT_FILENO);
            }

            char* args[MAXARGS];
            int i = 0;
            char* arg = strtok(cm, delim);
            while (i < MAXARGS && arg != NULL) {
                args[i] = arg;
                arg = strtok(NULL, delim);
                i++;
            }
            args[i] = NULL;
            char* cmd = args[0];

            int err = execvp(cmd, args);
            perror(cmd);
            _exit(err);
        } else {
            // Close not needed file decriptors;
            if (r_fd != -1)
                close(r_fd);
            if (pipefd[1] != -1)
                close(pipefd[1]);

            // The read end for the next processes to spawn.
            r_fd = pipefd[0];

            if (flg == -1)
                flg = PID;
            temp->pid = PID;
        }
        // Only take the first pid.
        temp = temp->Next;
    }
    // Just to be sure.
    if (r_fd != -1)
        close(r_fd);

    add_his(p, flg);
    // Now wait for Each processes to terminate.
    temp = start;
    while (temp != NULL) {
        // printf("waiting proc %s", temp->cmd);
        waitpid(temp->pid, NULL, 0);
        temp = temp->Next;
    }

    free_pipelist(start);
}

// Free a pipelist
void free_pipelist(struct PipeList* p)
{

    while (p != NULL) {
        struct PipeList* temp = p->Next;
        free(p->cmd);
        free(p);
        p = temp;
    }
}

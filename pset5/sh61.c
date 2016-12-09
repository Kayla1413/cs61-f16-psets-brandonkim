#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>

// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
    int background; // whether to run command in background
    command* next;    // Next command in list
    int conditional; // holds TOKEN_AND or TOKEN_OR for conditionals
    int exit_status; // holds exit status for use in conditionals
    int pipe_type; // Holds PIPE_IN/OUT/NONE, for use in pipes
    int pipe_fds[2]; // Holds pipe FDs, needed to close unneeded ends.
    int stdin_fd; // Holds fd for stdin. Will redirect stdin if positive.
    int stdout_fd; // Ditto stdout
    int stderr_fd; // Ditto stderr
};

// Current working directory global
char cwd[MAXPATHLEN];

// Built-in commands
const char BUILTIN_CD[] = "cd";
const char BUILTIN_EXIT[] = "exit";

// debug global
int debug;

// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
    c->background = 0;
    c->next = NULL;
    c->conditional = 0;
    c->exit_status = 42;
    c->pipe_type = PIPE_NONE;
    c->pipe_fds[0] = -1;
    c->pipe_fds[1] = -1;
    c->stdin_fd = STDIN_FILENO;
    c->stdout_fd = STDOUT_FILENO;
    c->stderr_fd = STDERR_FILENO;
    return c;
}

static void debug_print_command(command* c)
{
    printf("------------------DEBUG----------------------\n");
    printf("Command: ");
    for (int i=0; i < c->argc; ++i) {
        printf("%s ", c->argv[i]);
    }
    printf("\nargc: %d \n", c->argc);
    printf("PID: %d\n", c->pid);
    printf("Background: %d\n", c->background);
    if (c->next == NULL)
        printf("Next: NULL\n");
    else if (c->next->argc == 0)
        printf("Next: NULL\n");       
    else
        printf("Next: %p\n", c->next->argv[0]);
    if (c->conditional == TOKEN_AND)
        printf("Conditional: and\n");
    else if (c->conditional == TOKEN_OR)
        printf("Conditional: or\n");
    else
        printf("Conditional: none\n");
    printf("Exit Status: %d\n", c->exit_status);
    if (c->pipe_type == PIPE_IN)
        printf("Pipe: stdout\n");
    else if (c->pipe_type == PIPE_OUT)
        printf("Pipe: stdin\n");
    else
        printf("Pipe: none\n");
    printf("Pipe FDs: %d, %d\n", c->pipe_fds[0], c->pipe_fds[1]);
    printf("stdin: %d\nstdout: %d\nstderr: %d\n", c->stdin_fd, c->stdout_fd, c->stderr_fd);
    printf("-----------------DEBUG---------------------\n");
}

// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    for (int i = 0; i != c->argc; ++i)
        free(c->argv[i]);
    free(c->argv);
    free(c);
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
    pid_t child_pid;
    // Print debugging info if requested
    if (debug == 1)
        debug_print_command(c);
    // Handle built-ins before possibly forking
    if (strcmp(c->argv[0], BUILTIN_CD) == 0) {
        if (chdir(c->argv[1]) == -1) {
            perror("failed to change cwd: ");
            return -1;
        } else return 0;
    }
     if (strcmp(c->argv[0], BUILTIN_EXIT) == 0)
       _exit(EXIT_SUCCESS);

    child_pid = fork();
    if (child_pid == 0) {
        // In child
        c->pid = child_pid;
        // Handle pipes, redirections;
        dup2(c->stdin_fd, STDIN_FILENO);
        dup2(c->stdout_fd, STDOUT_FILENO);
        dup2(c->stderr_fd, STDERR_FILENO);
        execvp(c->argv[0], c->argv);

        // If connected to a pipe, close the other FD
        if (c->pipe_type == PIPE_IN)
            close(c->pipe_fds[0]);
        if (c->pipe_type == PIPE_OUT)
            close(c->pipe_fds[1]);
        // If this executes, something's gone wrong
        perror("Execvp failed: ");
        _exit(EXIT_FAILURE);
        return -1;
    } else if (child_pid > 0) {
	// In parent (shell)
        return c->pid;
    } else {
        perror("Failed to fork, oh dear: ");
        return -1;
    }
}


// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `set_foreground(pgid)` before waiting for the pipeline.
//       - Call `set_foreground(0)` once the pipeline is complete.
//       - Cancel the list when you detect interruption.

void run_list(command* c) {
    command* current;
    int status; // For waitpid()
    pid_t child;
    current = c;
    while (current->argc != 0) {
        child = start_command(current, 42); // Change the PGID later
        if (current->background == 0)
	    waitpid(child, &status, 0);
        if (current->next == NULL) // This was the last command in the list
	    break;

        //Otherwise we still have more to go, check conditionals
        if ((current->conditional == TOKEN_AND) && WIFEXITED(status) && (WEXITSTATUS(status)) == 0) {
            current = current->next;
            continue;
        } if (current->conditional == TOKEN_AND)
            break;
        if ((current->conditional == TOKEN_OR) && WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
            current = current->next;
            continue;
        } if (current->conditional == TOKEN_OR)
            break;
	if (current->conditional == 0) {
	    current = current->next;
	    continue;
	}
	// We should have continued by now...
	printf("Uncaught conditions Considered Harmful... \n");
    }
}


// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    command* next_command;
    int allocs = 0; // Track allocations of command structs to free later

    // Build the command
    command* c1 = command_alloc();
    command* current = c1;
    allocs = 1;
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {
        if (type == TOKEN_NORMAL)
            command_append_arg(current, token);
        if (type == TOKEN_BACKGROUND)
            current->background = 1;
        if (type == TOKEN_AND || type == TOKEN_OR)
            current->conditional = type;
        if (type == TOKEN_SEQUENCE || type == TOKEN_BACKGROUND || type == TOKEN_AND || type == TOKEN_OR) {
           next_command = command_alloc();
           allocs++;
           current->next = next_command;
           current = next_command;
        }
        if (type == TOKEN_PIPE) {
            next_command = command_alloc();
            allocs++;
            current->next = next_command;
            current->pipe_type = PIPE_IN;
            current->next->pipe_type = PIPE_OUT;
            current->background = 1;
            int fds[2];
            if (pipe(fds) == -1) {
                perror("Failed to create pipe: ");
                break;
            }
             if (memcpy(current->pipe_fds, fds, 2 * sizeof(int)) != current->pipe_fds) {
                 perror("Failed to copy pipe FDs to command struct: ");
                 break;
             }
            
           if (memcpy(next_command->pipe_fds, fds, 2 * sizeof(int)) != next_command->pipe_fds) {
                 perror("Failed to copy pipe FDs to command struct: ");
                 break;
             }
            current->stdout_fd = fds[1]; // 1st cmd's stdout will be write end of pipe
            current->next->stdin_fd = fds[0]; // 2nd cmd's stdin will be read end of pipe
            current = next_command;
        }


        waitpid(-1,0,WNOHANG);
    }
    // execute it
    if (c1->argc)
        run_list(c1);

   // free the list of commands, don't want leaky memory now do we?
   command** arr = malloc(allocs * sizeof(command*));
   int n = 0;
   current = c1;
   while (n != allocs) {
       arr[n] = current;
       current = current->next;
       n++;
   }
   while (n >= 1) {
       command_free(arr[n-1]);
       n--;
   }
   free(arr);
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option (and hackily not debug flag): read commands from file
    if (argc > 1 && strcmp(argv[1], "-d")) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
        debug = 1;

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_foreground(0);
    handle_signal(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Update cwd
        if (getcwd(cwd, MAXPATHLEN) == NULL) {
            perror("Unable to get current dir: ");
            return -1;
        } 
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]:%s:$ ", getpid(), cwd);
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file))
                    perror("sh61");
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
        waitpid(-1,0,WNOHANG);
    }

    return 0;
}

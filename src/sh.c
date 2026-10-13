/*
sh.c
CISC 361 Programming Project 2: Unix Shell
John Rellah III

The shell itself. sh() runs the read / parse / execute loop:
1. print the prompt "prefix [cwd] > "
2. read a line with fgets() (Ctrl-D / EOF is ignored)
3. split it into words with strtok()
4. run a built-in (exit,which,where,cd,pwd,list,pid,kill,prompt,printenv,setenv)
    or find the program in the PATH, expand * and ?
    wildcards, and run it with fork() / execve() / waitpid()


*/



#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "sh.h"

/* shell state */
static char prompt_display[PROMPTMAX + PATH_MAX + 8];
static struct pathelement *path_list = NULL;
static char prefix[PROMPTMAX + 1] = " ";
static char *prev_dir = NULL;
static char **shell_envp = NULL;

static const char *builtins[] = {
  "exit",
  "which",
  "where",
  "cd",
  "pwd",
  "list",
  "pid",
  "kill",
  "prompt",
  "printenv",
  "setenv",
  NULL
};

/* helper prototypes */
static void update_prompt(void);
static void free_path(struct pathelement *pathlist);
static char **parse_line(char *line, int *count);
static int is_builtin(const char *cmd);
static int is_executable_file(const char *path);
static int append_arg(char ***arr, size_t *n, size_t *cap, const char *s);
static char **expand_wildcards(char **args);
static void free_argv(char **argv);
static void run_external(char **args);

static void bi_which(char **args, int n);
static void bi_where(char **args, int n);
static void bi_cd(char **args, int n);
static void bi_pwd(void);
static void bi_list(char **args, int n);
static void bi_pid(void);
static void bi_kill(char **args, int n);
static void bi_prompt(char **args, int n);
static void bi_printenv(char **args, int n);
static void bi_setenv(char **args, int n);

/*
sh(): the main loop. Print prompt, read a line, split it into words,
run a built-in or external program, repeat until "exit".
*/


int sh( int argc, char **argv, char **envp )
{
  char line[MAXLINE];
  char **args;
  int nargs;
  int go = 1;

  (void)argc; (void)argv;
  shell_envp = envp;
  
    while ( go )
  {
    /* print your prompt */
    update_prompt();
    printf("%s", prompt_display);
    fflush(stdout);

    /* get command line and process */
    if(fgets(line, MAXLINE, stdin) == NULL) {
      if(feof(stdin)) {
        if(!isatty(STDIN_FILENO)) {
          break;
        }
        printf("\nUse \"exit\" to leave mysh.\n");
      }
      clearerr(stdin);
      continue;
    }

    /* line longer than our buffer: throw away the rest of it */
    if(strchr(line, '\n') == NULL && !feof(stdin)) {
      int c;
      while((c = getchar()) != '\n' && c != EOF);
      fprintf(stderr, "Line too long.\n");
      continue;
    }

    args = parse_line(line, &nargs);
    if (args == NULL) {
      continue;
    }
    if (nargs == 0) {
      free(args);
      continue;
    }

    /* check for each built in command and implement (like list method below) */
    if (is_builtin(args[0])) {
      printf("Executing built-in %s\n", args[0]);
      fflush(stdout);
    
      if(strcmp(args[0], "exit") == 0) {
      go = 0;
      }
      else if(strcmp(args[0], "which") == 0) {
      bi_which(args, nargs);
      }
      else if(strcmp(args[0], "where") == 0) {
      bi_where(args, nargs);
      }
      else if(strcmp(args[0], "cd") == 0) {
      bi_cd(args, nargs);
      }
      else if(strcmp(args[0], "pwd") == 0) {
      bi_pwd();
      }
      else if(strcmp(args[0], "list") == 0) {
      bi_list(args, nargs);
      }
      else if(strcmp(args[0], "pid") == 0) {
      bi_pid();
      }
      else if(strcmp(args[0], "kill") == 0) {
      bi_kill(args, nargs);
      }
      else if(strcmp(args[0], "prompt") == 0) {
      bi_prompt(args, nargs);
      }
      else if(strcmp(args[0], "printenv") == 0) {
      bi_printenv(args, nargs);
      }
      else if(strcmp(args[0], "setenv") == 0) {
      bi_setenv(args, nargs);
      }
    

     /*  else  program to exec */
    else {
      run_external(args);
    }

    free(args);
  
       /* find it */
       /* do fork(), execve() and waitpid() */

      /* else */
        /* fprintf(stderr, "%s: Command not found.\n", args[0]); */
    }

    /* clean up before leaving*/
    free_path(path_list);
    free(prev_dir);
  }
  return 0;
} /* sh() */

/* Prompt and parsing */

static void update_prompt(void) {
  char cwd[PATH_MAX];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    strcpy(cwd, "?");
  }
  snprintf(prompt_display, sizeof(prompt_display), "%s [%s]> ", prefix, cwd);
}

/* Splits line on spaces/tabs into a NULL-terminated array. The words point
  into line itself, so only the array needs to be freed.*/

static char **parse_line(char *line, int *count) {
  /* a line length L can hold at most L/2 + 1 words*/
  size_t max = strlen(line) / 2 + 2;
  char **args = malloc(max * sizeof(char *));
  char *tok;
  int n = 0;

  if (args == NULL) {
    perror("malloc");
    return NULL;
  }

  for (tok = strtok(line, " \t\n"); tok != NULL; tok = strtok(NULL, " \t\n")) {
    args[n++] = tok;
  }
  args[n] = NULL; /* execve needs the array to end with NULL*/

  *count = n;
  return args;
}

/* Returns 1 if cmd is one of this shell's built-in commands*/

static int is_builtin(const char *cmd) {
  int i;
  for (i = 0; builtins[i] != NULL; i++) {
    if (strcmp(cmd, builtins[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int is_executable_file(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISREG(st.st_mode) && access(path, X_OK) == 0;
}

/* Which / Where */

/* Returns a malloc'd copy of the FIRST executable match in the PATH,
or NULL. The caller must free() the result*/

char *which(char *command, struct pathelement *pathlist) {
  char buf[PATH_MAX];
  struct pathelement *p;

  if (command == NULL) {
    return NULL;
  }

  /* a name with a '/' in it is a path already, not something to search for.*/
  if(strchr(command, '/') != NULL) {
    return is_executable_file(command) ? strdup(command) : NULL;
  }

  for (p = pathlist; p != NULL; p = p->next){
    int len = snprintf(buf, sizeof(buf), "%s/%s", p->element, command);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
      continue;
    }
    if (is_executable_file(buf)) {
      return strdup(buf);
    }
  }
  return NULL;
}

/* Prints EVERY executable match in the path. Returns a malloc'd copy of 
the first match (or NULL); the caller must free() it. */

char *where(char *command, struct pathelement *pathlist) {
  char buf[PATH_MAX];
  char *first = NULL;
  struct pathelement *p;

  if (command == NULL) {
    return NULL;
  }

  for (p = pathlist; p != NULL; p = p->next) {
    int len = snprintf(buf, sizeof(buf), "%s/%s", p->element, command);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
      continue;
    }
    if (is_executable_file(buf)) {
      printf("%s\n", buf);
      if (first == NULL) {
        first = strdup(buf);
      }
    }
  }
  return first;
}

/* Built in which: for each argument, print the first matching executable 
in the PATH (or say it is a shell built-in / not found)*/

static void bi_which(char **args, int n) {
  int i;
  if (n < 2) {
    fprintf(stderr, "which: Too few arguments\n");
    return;
  }
  for (i = 1; i < n; i++) {
    char *path;
    if (is_builtin(args[i])) {
      printf("%s: shell built-in command\n", args[i]);
      continue;
    }
    path = which(args[i], path_list);
    if (path != NULL) {
      printf("%s\n", path);
      free(path);
    } else {
      printf("%s: Command not found.\n", args[i]);
    }
  }
}

/* Built in where: for each argument, print EVERY matching executable
 in the PATH (and note if it is also a shell built-in)*/

 static void bi_where(char **args, int n) {
  int i;
  if (n < 2) {
    fprintf(stderr, "where: Too few arguments\n");
    return;
  }
  for (i = 1; i < n; i++) {
    char *first;
    if (is_builtin(args[i])) {
      printf("%s: is a shell built-in\n", args[i]);
    }
    first = where(args[i], path_list);
    free(first);

  }
 }

 /* Running external programs */
 /* Runs a non-built-in command: finds the program (path given or PATH search)
 , expands wildcards, prints "Executing <path>", then fork()s.
 The child execve()s the program, the parent waitpid()s for it and
 prints the exit status if it is not zero*/

 static void run_external(char **args) {
  char *cmdpath;
  char **argv_exp;
  pid_t pid;
  int status = 0;
  int waited = 0;

  /* 1. Find the program*/
  if (strchr(args[0], '/') != NULL) {
    /* absolute or relative path: /bin/ls, ./ls, ../bin/ls ...*/
    struct stat st;
    if (stat(args[0], &st) != 0) {
      fprintf(stderr, "%s: Command not found.\n", args[0]);
      return;
    }
    if (S_ISDIR(st.st_mode) || access(args[0], X_OK) != 0) {
      fprintf(stderr, "%s: Permission denied.\n", args[0]);
      return;
    }
    cmdpath = strdup(args[0]);
  } else {
    /* plain name: search the PATH list*/
    cmdpath = which(args[0], path_list);
    if (cmdpath == NULL) {
      fprintf(stderr, "%s: Command not found.\n", args[0]);
      return;
    }
  }
  if (cmdpath == NULL) {
    perror("strdup");
    return;
  }

  /* 2. Expand * and ? wildcards into real file names. */
  argv_exp = expand_wildcards(args);
  if (argv_exp == NULL) {
    free(cmdpath);
    return;
  }

  /* 3. Say what we're about to run, then fork/execve/waitpid*/
  printf("Executing %s\n", cmdpath);
  fflush(stdout); /* flush BEFORE fork, or the child repeats the output */

  pid = fork();
  if (pid < 0) {
    perror("fork");
  } else if (pid == 0) {
    /* Child: put signals back to normal so Ctrl-C / kill work on it.
     (Ctrl-Z stays ignored: this shell has no job control, so a stopped
     child would hang the shell forever)*/
     signal(SIGINT, SIG_DFL);
     signal(SIGTERM, SIG_DFL);
     execve(cmdpath, argv_exp, shell_envp);
     perror(cmdpath);
     _exit(127);
  } else {
    /* Parent: wait for the child. waitpid can be interrupted by our own Ctrl-C
    handler, so try again if errno is EINTR*/
    while(waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR) {
        perror("waitpid");
        break;
      }
    }
    waited = 1;
  }
  /* 4. Report a not zero exit status like tcsh's printexitvalue. */
  if (waited) {
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      printf("Exit %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) == SIGINT) {
        printf("\n");
      } else {
        printf("%s\n", strsignal(WTERMSIG(status)));
      }
    }
    
  } 
  free_argv(argv_exp);
  free(cmdpath);

 }

 /* run_external() */
 /* Adds a copy of s to a growable NULL-terminated array */
 static int append_arg(char ***arr, size_t *n, size_t *cap, const char *s) {
  if (*n + 1 >= *cap) { /* +1 keeps room for the final NULL*/
    size_t newcap = *cap * 2;
    char **tmp = realloc(*arr, newcap * sizeof(char *));
    if (tmp == NULL) {
      perror("realloc");
      return -1;
    }
    *arr = tmp;
    *cap = newcap;
  }
  (*arr)[*n] = strdup(s);
  if ((*arr)[*n] == NULL) {
    perror("strdup");
    return -1;
  }
  (*n)++;
  (*arr)[*n] = NULL;
  return 0;
 }

 /* Wildcards: builds a new argv where every argument containing * or ? is
  replaced by the sorted list of file names glob(3) matches. GLOB_NOCHECK
  makes glob hand back the original text when nothing matches, so the
  original argument is passed to execve() unchanged. args[0] (the command itself)
  is never expanded. The result must be freed with free_argv().*/

static char **expand_wildcard(char **args) {
  
}



void list ( char *dir )
{
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
} /* list() */


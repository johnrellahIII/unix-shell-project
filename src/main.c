/*
main.c
CISC 361 Programming Project 2: Unix Shell
John Rellah III

Sets up signal handling, then starts the shell loop in sh().
  - Ctrl-C (SIGINT) is caught, ignored at the prompt, but still kills a running command
  - Ctrl-Z (SIGTSTP) and SIGTERM are ignored by the shell
*/

#include "sh.h"
#include <signal.h>
#include <stdio.h>

void sig_handler(int signal); 

int main( int argc, char **argv, char **envp )
{
  /*
  Ctrl-C (SIGINT) is CAUGHT, not ignored. A caught signal is reset to its
  default action when a child calls execve(), so a running program still
  dies on Ctrl-C while the shell itself survives.
  */
 if (signal(SIGINT, sig_handler) == SIG_ERR) {
    perror("signal(SIGINT)");
  }
  /*
  Ctrl-Z(SIGTSTP) and SIGTERM: ignored by the shell.
  */
 if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
    perror("signal(SIGTSTP)");
  }
 if (signal(SIGTERM, SIG_IGN) == SIG_ERR) {
    perror("signal(SIGTERM)");
  }

return sh(argc, argv, envp);
}
/*
SIGINT handler. It does nothing on purpose: catching Ctrl-C 
(instead of letting it kill us) is what keeps the shell alive. A running command
still dies, because execve() resets caugh singals to their default.
*/
void sig_handler(int signal)
{
  (void)signal;
}


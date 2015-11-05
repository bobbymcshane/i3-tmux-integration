#define _XOPEN_SOURCE 600 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>

int main() {
     pid_t i;
     char buf[10];
     int fds, fdm, status;

     /* Open a new unused tty */
     fdm = posix_openpt(O_RDWR);
     grantpt(fdm);
     unlockpt(fdm);

     printf("pts/%s\n", ptsname(fdm));
     close(0);
     close(1);
     close(2);

     i = fork();
     if ( i == 0 ) { // parent
          dup(fdm);
          dup(fdm);
          dup(fdm);
          printf("Where do I pop up?\n");
          sleep(2);
          printf("Where do I pop up - 2?\n");
          waitpid(i, &status, 0);
     } else {  // child
          fds = open(ptsname(fdm), O_RDWR);
          dup(fds);
          dup(fds);
          dup(fds);
          strcpy(buf, ptsname(fdm));
          /* Spawn a urxvt terminal which looks at the specified pty */
          sprintf(buf, "urxvt -pty-fd %c", basename(buf));
          system(buf);
          exit(0);
     }
}

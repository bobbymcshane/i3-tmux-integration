#define _XOPEN_SOURCE 600 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <pty.h>

int main() {
     pid_t i;
     char buf[10];
     char buf2[10];
     int fds, fdm, status;

     /* Open a new unused tty */
     openpty( &fdm, &fds, NULL, NULL, NULL );
     //grantpt(fdm);
     //unlockpt(fdm);

     printf("%s\n", ptsname(fdm));

     i = fork();
     if ( i == 0 ) { // parent
          dprintf(fdm,"Where do I pop up?\n");
          /*sleep(2);
          dprintf("Where do I pop up - 2?\n");*/
          waitpid(i, &status, 0);
     } else {  // child
          //close(0);
          //close(1);
          //close(2);
          /*dup(fds);
          dup(fds);
          dup(fds);*/
          strcpy(buf, ptsname(fdm));
          /* Spawn a urxvt terminal which looks at the specified pty */
          sprintf(buf2, "/usr/bin/urxvt -pty-fd %d", fds);
          printf("%s\n",buf2);
          system(buf2);
          /*if (execlp("/usr/bin/xterm", "-S", "ab%d", NULL)==-1) {
               printf ("ERROR\n");
          }*/
          exit(0);
     }
}

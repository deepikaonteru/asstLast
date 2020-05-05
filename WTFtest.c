#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>


void  ClientProcess(void)
{
    system("./WTF configure 127.0.0.1 9123");

    system("./WTF create WTFProject")
}

void  ServerProcess(void)
{
    system("./WTF 9123");

}


void  main(void)
{
     pid_t  pid;

     pid = fork();
     if (pid == 0) 
          ClientProcess();
     else 
          ServerProcess();
}

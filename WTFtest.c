#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

void serverProcess(pid_t pid)
{
    system("./serverSide/WTFserver 9726");
}

void main(void)
{
    system("./clientSide/WTF configure 127.0.0.1 9726");

    pid_t pid;
    pid = fork();
    if(pid == 0) {
        serverProcess(pid);
    }
    else {
        system("./clientSide/WTF create CS111");
        system("echo asst1 > ./clientSide/clientRepos/CS111/asst1.txt");
        system("echo asst2 > ./clientSide/clientRepos/CS111/asst2.txt");
        system("./clientSide/WTF add CS111 asst1.txt");
        system("./clientSide/WTF add CS111 asst2.txt");
        system("./clientSide/WTF commit CS111");
        system("./clientSide/WTF push CS111");
        system("./clientSide/WTF update CS111");
        system("./clientSide/WTF upgrade CS111");

        kill(getpid(), SIGKILL);
    }
        
}
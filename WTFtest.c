#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "functionHeaders.h"

void upgrade(char *project)
{
	//create a child process to call the function on the server running on the parent
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "upgrade" ,project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	//get the status of the child to wait for it to completely finish
	//other functions are the exact same for the other functions of the project
	int status;
	waitpid(child, &status, 0);
}

void push_to_server(char *project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "push" ,project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void commit(char *project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "commit" ,project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void history(char *project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "history",project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}


void update(char *project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "update",project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void rollback(char *project, char* version)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "rollback",project, version, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}


void current_version(char* project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "currentversion", project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void remove_manifest(char* project, char* file)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "remove", project, file, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void add(char* project, char* file)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "add", project, file, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void destroy(char* project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "destroy", project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}


void create(char* project)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "create", project, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void configure_client(char* ip, char* portno)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./clientSide/WTF", "configure", ip, portno, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

void configure_server(char* portno)
{
	pid_t child;
	if((child = fork()) == 0)
	{
		char *args[] = {"./serverSide/WTFserver", portno, (char*)0};

		execv(args[0], args);
		perror("Execv error child_1");
		_exit(1);
	}
	int status;
	waitpid(child, &status, 0);
}

int main(int argc, char **argv)
{
	char* portno = "28312";
	pid_t pid = fork();
	int pid1;
	if(pid < 0)
	{
		//fork failed so print error
		perror("fork failed.\n");
		exit(1);
	}
	else if(pid == 0)
	{
		//set up server 
		pid1 = getpid();
		configure_server(portno);
	}
	else
	{
		//parent process
		//this is where all of the tests are called to see if the functions work properly
		//kill the child process
		configure_client("snow.cs.rutgers.edu", portno);
		create("HELLO");		
		create("GOODBYE");
		destroy("GOODBYE");
		int fd1 = open("./clientSide/clientRepos/HELLO/test.txt", O_CREAT | O_WRONLY | O_TRUNC, 0777);
		int fd2 = open("./clientSide/clientRepos/HELLO/test1.txt", O_CREAT | O_WRONLY | O_TRUNC, 0777);
		add("HELLO", "test.txt");
		add("HELLO", "test1.txt");
		//commit("HELLO");
		//push_to_server("HELLO");
		remove_manifest("HELLO", "test1.txt");
		update("HELLO");
		upgrade("HELLO");
		current_version("HELLO");
		//rollback("HELLO", "1");
		//history("HELLO");
		close(fd1);
		close(fd2);
		//kill the server after all of the functions have been run
		kill(pid, SIGINT);
	}
	int status;
	waitpid(pid1, &status, 0);
    return 0;
}
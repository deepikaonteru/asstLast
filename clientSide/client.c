// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <unistd.h> 
#include <stdlib.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <string.h> 

void configure(char* hostName, char* port)
{
    remove("./.configure");
    int fd = open("./.configure", O_WRONLY | O_CREAT, 00600);
    write(fd, hostName, strlen(hostName));
    write(fd, "\n", 1);
    write(fd, port, strlen(port));
    close(fd);
}

char* getHostName(int fd)
{
    char buffer[256];
    memset(buffer, '\0', 256);
    int bytesRead;
    int index = 0;

    do
    {
        char c = 0;
        bytesRead = read(fd, &c, sizeof(char));
        //CHECKS
        if(c == '\n')
        {
            break;
        }
        else
        {
            buffer[index] = c;
            index ++;
        }
    } while(bytesRead > 0);

    return buffer;
}

int getPortNum(int fd)
{
	char buffer[10];
    memset(buffer, '\0', 10);
	int bytesRead;

    //ip/hostname
	do 
    {
		char c = 0;
		bytesRead = read(fd, &c, sizeof(char));
        //printf("%d\n", bytesRead);
		//CHECKS
        if(c == '\n')
        {
            break;
        }
	} while(bytesRead > 0);

    //port#
    int index = 0;
    do
    {
        char c = 0;
        bytesRead = read(fd, &c, sizeof(char));
        buffer[index] = c;
        index ++;
    } while(bytesRead > 0);

    return atoi(buffer);
}

void create(char* projName)
{
    int sock, port, numBytes;
    struct sockaddr_in serverAddr;
    struct hostent* server;

    //read from .configure file to obtain IP and port#
    int fd = open("./.configure", O_RDONLY);
        if(fd == -1) { //if file does not exist
		printf("Error: missing .configure\n");
		close(fd);
		return;
	}

    fd = open("./.configure", O_RDONLY);
    port = getPortNum(fd);
    close(fd);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        printf("Error: socket failed to open.\n");
        return;
    }

    fd = open("./.configure", O_RDONLY);
    server = gethostbyname(getHostName(fd));

}

//First step: ./client configure argv[2], argv[3] 
//Next step: ./client create argv[2]

int main(int argc, char *argv[]) 
{ 
    //configure step
    if(strcmp(argv[1], "configure") == 0)
    {
        configure(argv[2], argv[3]);
    }

    //create step
    else if(strcmp(argv[1], "create") == 0)
    {
        create(argv[1]);
    }

    //add step
    //case 1: adding file for the first time to local repo
    //case 2: adding a modified file to local repo
    //consider: .Manifest does not exist yet
    
    
    /*
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char *hello = "Hello from client"; 
    char buffer[1024] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 
    send(sock , hello , strlen(hello) , 0 ); 
    printf("Hello message sent\n"); 
    valread = read( sock , buffer, 1024); 
    printf("%s\n",buffer ); 
    return 0;
    */
} 
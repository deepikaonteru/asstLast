// Server side C/C++ program to demonstrate Socket programming 
#include <unistd.h> 
#include <stdio.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
int main(int argc, char *argv[]) 
{ 
    if(argc != 2)
    {
        printf("Error: Invalid number of arguments.\n");
        return -1;
    }
    else
    {
        int serverSocket, newSocket, port, clientAddrSize, numBytesRead;
        char buffer[1024];
        memset(buffer, '\0', 1024);

        struct sockaddr_in serverAddr, clientAddr;

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if(serverSocket < 0)
        {
            printf("Error: socket failed to open.\n");
            return -1;
        }

        bzero((char*)(&serverAddr), sizeof(serverAddr));

        port = atoi(argv[1]);

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if(bind(serverSocket, (struct sockaddr*)(&serverAddr), sizeof(serverAddr)) < 0)
        {
            printf("Error: binding failed.\n");
            return -1;
        }

        listen(serverSocket, 5);

        while(1)
        {
            clientAddrSize = sizeof(clientAddr);
            newSocket = accept(serverSocket, (struct sockaddr*)(&clientAddr), &clientAddrSize);
            if(newSocket < 0)
            {
                printf("Error: accepted socket failed to open.\n");
                //return -1;
            }
            else
            {
                printf("Connected to client.\n");
                numBytesRead = read(newSocket, buffer, 1023);
                if(numBytesRead < 0)
                {
                    printf("Error: couldn't read from socket.\n");
                }
                else
                {
                    printf("Message from Client: %s.\n", buffer);
                }
            }
        }
    }
    /*
    //check if port was specified in command line, otherwise program will SEGFAULT
    if(argc != 2)
    {
        printf("Error: Need to specify port #.\n");
        return -1;
    }

    int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    char *hello = "Hello from server"; 
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY;
    unsigned short int port = (unsigned short int)(atoi(argv[1]));
    address.sin_port = htons( port ); 
       
    // Forcefully attaching socket to the port 8080 
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(server_fd, 3) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) 
    { 
        perror("accept"); 
        exit(EXIT_FAILURE); 
    } 
    valread = read( new_socket , buffer, 1024); 
    printf("%s\n",buffer ); 
    send(new_socket , hello , strlen(hello) , 0 ); 
    printf("Hello message sent\n"); 
    return 0; 
    */
} 
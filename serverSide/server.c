#include <unistd.h> 
#include <stdio.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h> 
#include <string.h>
#include "socketBuffer.h"
char SERVER_REPOS[] = "./serverRepos";
char DOT_VERSION[] = ".version";
char DOT_HISTORY[] = ".history";


long int findFileSize(char *fileName) {
		
	struct stat buffer;
	int status = stat(fileName, &buffer);
	// if permission available
	if(status == 0) {
		return buffer.st_size;
	}
	return -1;
}

char *readFileContents(char *fileName) {
	if(!fileExistsCheck(fileName)) {
		printf("Unable to read file contents. File %s does not exist.", fileName);
		return NULL;
	}
	char *result = malloc(sizeof(char) * (findFileSize(fileName) + 10));
	
	// Now write file char by char on the socket		
	int fileFd = open(fileName, O_RDONLY, 0777);
	int i = 0;
	while (read(fileFd, &result[i], 1) == 1) {
		i++;
	}

	close(fileFd);
	result[i] = '\0';
	return result;

}

char *readCurrentVersion(char *projName) {
	char *path = malloc(sizeof(char) * (strlen(projName) + strlen(DOT_VERSION) + 40 + strlen(SERVER_REPOS)));
	sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, DOT_VERSION);
	
	char *result = readFileContents(path);
	free(path);
	return result;
}

int fileExistsCheck(char *fileName) {
	struct stat buffer;
	return (stat(fileName, &buffer) == 0);
}

void writeFileToSocket(int sock, char *projName, const char *filePath) {
	
	// Check if project exists.
	printf("Writing File %s in project: %s to client.\n", filePath, projName);
	char *path = malloc(sizeof(char) * (strlen(projName) + strlen(filePath) + 50 + strlen(SERVER_REPOS)));
	
	// Read current version of project.
    char *version = readCurrentVersion(projName);
	
	// file path
	sprintf(path, "%s/%s/%s/%s", SERVER_REPOS, projName, version, filePath);
	free(version);
	
	// Check if file exists.
	if(!fileExistsCheck(path)) {
		printf("File does not exist: %s\n", path);
	} else {
		
		long fileSize = findFileSize(path);				
		int fileFd = open(path, O_RDONLY, 0777);
		
		sprintf(path, "%d:%s%ld:", strlen(filePath), filePath, fileSize);
		write(sock, path, strlen(path));
		
		// Now write file char by char on the socket	
		char c;
		while (read(fileFd, &c, 1) == 1) {
			write(sock, &c, 1);
		}
        
		close(fileFd);
	}

	free(path);
}

// <failed: error:
void writeErrorToSocket(int sock, char *error) {
	write(sock, "failed:", strlen("failed:"));
	write(sock, error, strlen(error));
	write(sock, ":", 1);
}

void serverDestroy(char* projName, int sock){

    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));

    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
    //printf("%s\n",path);

    if(!fileExistsCheck(path)) 
    {
		writeErrorToSocket(sock, "Project does not exist.");
	} 
    else 
    {
		char *path = malloc(sizeof(char) * (strlen(projName) + 50 + strlen(SERVER_REPOS)));
		sprintf(path, "%s/%s", SERVER_REPOS, projName);
			
        // ADD A REMOVE DIR FUNCTION THAT TAKES IN PATH
            
        free(path);
			
		write(sock, "destroyed:", strlen("destroyed:")); 
	}
		
}

void serverCreate(char* projName, int sock)
{
    //Step 1: attempt to make directory.
    // - If mkdir() returns 0, it made a directory
    // - If mkdir() returns -1, it failed to make a directory, check errno and report that to client
    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
    //printf("%s\n",path);

    int mkdirResult = mkdir(path, 00777);
    if(mkdirResult == -1)
    {
        //did not make it, check errno for if it was already existing, then report that to client
        if(errno == EEXIST)
        {
            //printf("Error: project %s already exists on server.\n", projName);
            //char* errCode = "400";
            //send(sock, errCode, strlen(errCode), 0);
            writeErrorToSocket(sock, "Project Already exists.");
        }  
    }
    else if(mkdirResult == 0)
    {
        //made it, now initialize .Manifest
        //char* msgCode = "200";
        //send(sock, msgCode, strlen(msgCode), 0);

        char version[10] = "1";

        //make version file
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, DOT_VERSION);
        int vfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
        write(vfd, version, strlen(version));
        close(vfd);

        // make history file 
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, DOT_HISTORY);
        int hfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
        close(hfd);
	
        // create version directory
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, version);
        mkdir(path, 00777);

        /*char* pathToManifest = (char*)(malloc((strlen(path) + strlen("/.Manifest")) * sizeof(char)));
        strcpy(pathToManifest, path);
        strcat(pathToManifest, "/.Manifest");*/

        // place .Manifest inside version directory
        sprintf(path, "%s/%s/%s/%s", SERVER_REPOS, projName, version, ".Manifest");

        int manifestFD = open(path, O_WRONLY | O_CREAT, 00600);
        write(manifestFD, "1\n", 2);
        write(sock, "sendfile:", strlen("sendfile:"));
	    write(sock, "1:", 2); 
		writeFileToSocket(sock, projName, path);

    }
}

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
            }
            else
            {
                //printf("Connected to client.\n");
                //Step 1: Read byte-by-byte until hits first colon, then use atoi() to get length of rest of command
                //***DO NOT SAVE THE FIRST COLON WHEN USING atoi()
                char lenBuf[10];
                memset(lenBuf, '\0', 10 * sizeof(char));
                int n; //bytesRead
                int index = 0;
                do {
                    char c = 0;
                    n = read(newSocket, &c, sizeof(char));

                    //checks
                    if(c == ':')
                    {
                        break;
                    }
                    else
                    {
                        lenBuf[index] = c;
                        index ++;
                    }
                } while(n > 0);
                int lenCmd = atoi(lenBuf);
                //printf("%d\n", lenCmd);

                //Step 2: Read 'lenCmd' number of bytes of the socket, save to a buffer
                char* fullCmdBuf = (char*)(malloc((lenCmd + 1) * sizeof(char)));
                n = read(newSocket, fullCmdBuf, lenCmd * sizeof(char));
                strcat(fullCmdBuf, "\0");
                
                //Step 3: Loop through cmdBuf until FIRST colon is found. Everything to the left is 
                //the actual operation
                char* cmdBuf = (char*)(malloc(lenCmd * sizeof(char)));
                memset(cmdBuf, '\0', lenCmd * sizeof(char));
                int i = 0;
                while(fullCmdBuf[i] != ':')
                {
                    cmdBuf[i] = fullCmdBuf[i];
                    i ++;
                }
                printf("%s\n", cmdBuf);

                //Step 4: Determine what command is in cmdBuf. Based on that command, do said action.
                if(strcmp(cmdBuf, "cr") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverCreate(projName, newSocket);
                }

                if(strcmp(cmdBuf, "des") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    printf("%s\n",projName);
                    serverDestroy(projName, newSocket);
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
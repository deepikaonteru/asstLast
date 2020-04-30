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
#include "socketBufferC.h"
char CLIENT_REPOS[] = "./clientRepos";

void writeFileFromSocket(int sockToRead, char *projName) { //lenOfFileName:fileName:sizeOfFile:contentsOfFile
                                                           //9:.Manifest:
	SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sockToRead, ':');
	char *nameLenStr = readAllBuffer(socketBuffer);
	long nameLen = atol(nameLenStr);
	
	readNBytes(socketBuffer, sockToRead, nameLen);
	char *fileName = readAllBuffer(socketBuffer);
	
    //skip colon after fileName
    readTillDelimiter(socketBuffer, sockToRead, ':');

	readTillDelimiter(socketBuffer, sockToRead, ':');
	char *contentLenStr = readAllBuffer(socketBuffer);
	long contentLen = atol(contentLenStr);
    //printf("%s\n", contentLenStr);

    char* contentOfFile = (char*)(malloc(sizeof(char) * contentLen));
    int contentBytesRead = read(sockToRead, contentOfFile, contentLen);

    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
	sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);
    int projDir = mkdir(pathToProject, 00777);
		
	//char *fullpath = malloc(sizeof(char) * (strlen(fileName) + 15 + strlen(projName)));
    char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(fileName))));
	sprintf(pathToManifest, "%s/%s/%s", CLIENT_REPOS, projName, fileName);

	// Write data to the file now.
	int clientManifest = open(pathToManifest, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    write(clientManifest, contentOfFile, contentBytesRead);
    
    /*
	char c;
	long i = 0;
	while(i < contentLen) {
		read(sockToRead, &c, 1);
		if(c == '\0') {
			break; // Socket disconnected.
		}
		write(fd, &c, 1);
        i ++;
	}
    */
	close(clientManifest);	
	
	// de-allocate memory
	freeSocketBuffer(socketBuffer);
	free(nameLenStr);
	free(fileName);
	free(contentLenStr);
    free(contentOfFile);
    free(pathToProject);
	free(pathToManifest);
}

void configure(char* hostName, char* port)
{
    remove("./.configure");
    int fd = open("./.configure", O_WRONLY | O_CREAT, 00600);
    write(fd, hostName, strlen(hostName));
    write(fd, "\n", 1);
    write(fd, port, strlen(port));
    close(fd);
}

void getHostName(int fd, char* buffer)
{
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

int DirExistsCheck(char *dirName) {

    DIR* dir = opendir(dirName);
    if (dir) {
        /* Directory exists. */
        return 1;
        closedir(dir);
    } else {
        return 0;
    }

}

int fileExistsCheck(char* fName)
{
    struct stat buf;
    int res = stat(fName, &buf);
    if(res == 0)
    {
        return 1; //file exists
    }
    else
    {
        return 0; //file does not exist
    }
}

int projExistsInClient(char *projName) {
	
	// Check if project exists
	if(!DirExistsCheck(projName)) {
		printf("Error: The project does not exist.\n");
		return 0;
	}
	
	char *path = malloc(sizeof(char) * (strlen(projName) + 50));
	
	// check if project directory contains manifest
	sprintf(path, "%s/%s", projName, "./Manifest");
	if(!fileExistsCheck(path)) {
		printf("Error: Please create project first.\n");
		free(path);
		return 0;
	}
	return 1;
}

// add an entry for the the file
// to its client's .Manifest with a new version number and hashcode
void add(char* projName, char* filePath)
{
    // Check if project exists
	if(!projExistsInClient(projName)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}

    char *path = malloc((strlen("./clientRepos")* (strlen(projName) + 50)) * sizeof(char));

    // check if given file exists
	sprintf(path, "%s/%s", projName, filePath);
	if(!fileExistsCheck(path)) {
		printf("Error: File does not exist locally in project.\n");
		printf("Cannot to find: %s\n", path);
		free(path);
		return;
	}

    // check if project directory contains manifest
	sprintf(path, "%s/%s", projName, ".Manifest");
    // Make changes to manifest: add entry with new version number and hashcode
	int manifestFD = open(path, O_RDONLY, 0777);

}

void destroy(char* projName)
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
        char hostName[256];
        getHostName(fd, hostName);
        server = gethostbyname(hostName);
        if(server == NULL)
        {
            printf("Error: no such host.\n");
            return;
        }

        bzero((char*)(&serverAddr), sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        bcopy((char*)(server->h_addr), (char*)(&serverAddr.sin_addr.s_addr), server->h_length);
        serverAddr.sin_port = htons(port);

        if(connect(sock, (struct sockaddr*)(&serverAddr), sizeof(serverAddr)) < 0)
        {
            printf("Error: could not connect.\n");
            return;
        }

        printf("Sending Project: %s to destroy\n", projName);

        // des:<projectNameLength>:<projectName>
        char* baseCmd = (char*)(malloc((strlen("des:") + strlen(projName) + 1) * sizeof(char)));
        strcpy(baseCmd, "des:");
        strcat(baseCmd, projName);
        strcat(baseCmd, "\0");
        
        int numBytesToSend = strlen(baseCmd);
        char numBytesBuffer[10];
        memset(numBytesBuffer, '\0', 10 * sizeof(char));
        sprintf(numBytesBuffer, "%d", numBytesToSend);

        char* fullCmd = (char*)(malloc((strlen(numBytesBuffer) + 1 + strlen(baseCmd) + 1) * sizeof(char)));
        strcpy(fullCmd, numBytesBuffer);
        strcat(fullCmd, ":");
        strcat(fullCmd, baseCmd);
        strcat(fullCmd, "\0");
        //printf("%s\n", fullCmd);

        free(baseCmd);
        
        send(sock, fullCmd, strlen(fullCmd), 0);
        // Server Sends back SocketBuffer that says destroyed: blah blah 
        // or "failed:<fail Reason>:"
        SocketBuffer *socketBuffer = createBuffer();
        
        readTillDelimiter(socketBuffer, sock, ':');
        char *responsefrmServer = readAllBuffer(socketBuffer);
        
        if(strcmp(responsefrmServer, "destroyed") == 0) {
            printf("Project %s destroyed successfully\n",projName);
            
        } else {
            // failed to destroy
            printf("Project could not be destroyed on server.\n");		
            readTillDelimiter(socketBuffer, sock, ':');
            char *reason = readAllBuffer(socketBuffer);
            printf("Reason: %s\n", reason);
            free(reason);
        }
        freeSocketBuffer(socketBuffer);
        free(responsefrmServer);

}


void getCurrentVersion(char* projName)
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
    char hostName[256];
    getHostName(fd, hostName);
    server = gethostbyname(hostName);
    if(server == NULL)
    {
        printf("Error: no such host.\n");
        return;
    }

    bzero((char*)(&serverAddr), sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char*)(server->h_addr), (char*)(&serverAddr.sin_addr.s_addr), server->h_length);
    serverAddr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr*)(&serverAddr), sizeof(serverAddr)) < 0)
    {
        printf("Error: could not connect.\n");
        return;
    }

     printf("Trying to get current version of Project: %s.\n", projName);


     // currentversion:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(projName) + 50));;
	sprintf(command, "%s:%d:%s", "currVer", strlen(projName), projName);
	write(sock, command, strlen(command));
	free(command);

    // server sends back 
    // sendfile:<numFiles>:<ManifestNameLen>:<manifest name><numBytes>:<contents>
	// OR "failed:<fail Reason>:"
 
    SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sock, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		
        //The client should output a list of all
        //files under the project name, along with their version number (i.e., number of updates).
		
	} else {
		printf("Could not get project version from server.\n");		
		readTillDelimiter(socketBuffer, sock, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	freeSocketBuffer(socketBuffer);
	free(responseCode);

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
    char hostName[256];
    getHostName(fd, hostName);
    server = gethostbyname(hostName);
    if(server == NULL)
    {
        printf("Error: no such host.\n");
        return;
    }

    bzero((char*)(&serverAddr), sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char*)(server->h_addr), (char*)(&serverAddr.sin_addr.s_addr), server->h_length);
    serverAddr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr*)(&serverAddr), sizeof(serverAddr)) < 0)
    {
        printf("Error: could not connect.\n");
        return;
    }

    /*Test Send Command: THIS WORKS*/
    //char* msgToServer = "yerrrrr\0";
    //send(sock, msgToServer, strlen(msgToServer), 0);

    //Send a message to the server with the name of the proj. to be created
    //createProject:<lengthOfProjName>:projName

    /*
    int lenProjName = strlen(projName);
    //printf("%s\n", (char*)(lenProjName));//HOW TO CONVERT TO STRING???


    char* msg = (char*)(malloc((strlen("createProject:") + strlen(projName)) * sizeof(char)));
    strcpy(msg, "createProject:");
    //strcat(msg, );
    strcat(msg, projName);
    send(sock, msg, strlen(msg), 0);
    */
    char* baseCmd = (char*)(malloc((strlen("cr:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "cr:");
    strcat(baseCmd, projName);
    strcat(baseCmd, "\0");
    
    int numBytesToSend = strlen(baseCmd);
    char numBytesBuffer[10];
    memset(numBytesBuffer, '\0', 10 * sizeof(char));
    sprintf(numBytesBuffer, "%d", numBytesToSend);

    char* fullCmd = (char*)(malloc((strlen(numBytesBuffer) + 1 + strlen(baseCmd) + 1) * sizeof(char)));
    strcpy(fullCmd, numBytesBuffer);
    strcat(fullCmd, ":");
    strcat(fullCmd, baseCmd);
    strcat(fullCmd, "\0");
    //printf("%s\n", fullCmd);

    free(baseCmd);
    
    send(sock, fullCmd, strlen(fullCmd), 0);

    //expect some message in return, or some .Manifest, then create project locally
    SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, sock, ':');
	char *responsefrmServer = readAllBuffer(socketBuffer);
	
	if(strcmp(responsefrmServer, "sendfile") == 0) {
		readTillDelimiter(socketBuffer, sock, ':');
		char *numFilesStr = readAllBuffer(socketBuffer);
		long numFiles = atol(numFilesStr);
		
		// Now read N files, and save them
		// for create, only 1 file will be sent, the .Manifest
		while(numFiles > 0) {
			writeFileFromSocket(sock, projName);
            numFiles --;
		}
		printf("Project %s created.\n", projName);
		free(numFilesStr);
		
	} else {
        //failed
        printf("Could not create project on server.\n");		
		readTillDelimiter(socketBuffer, sock, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	freeSocketBuffer(socketBuffer);
	free(responsefrmServer);

    /*char resultCode[4];
    memset(resultCode, '\0', 4);
    read(sock, resultCode, 3);
    printf("%d\n", atoi(resultCode));*/

    return; 
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
        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            create(argv[2]);
		}
    }

    //currentVersion step
    else if(strcmp(argv[1], "currentVersion") == 0)
    {
        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            getCurrentVersion(argv[2]);
		}
    }

    //destroy step
    else if(strcmp(argv[1], "destroy") == 0)
    {
        destroy(argv[2]);
    }
    //add step
    else if(strcmp(argv[1], "add") == 0)
    {
        add(argv[2], argv[3]);
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
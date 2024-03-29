#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <unistd.h> 
#include <stdlib.h>
#include <dirent.h>
#include <openssl/md5.h>
#include <fcntl.h>
#include <string.h> 
#include "socketBufferC.h"
#include "zlibCompDecomp.h"
char CLIENT_REPOS[] = "./clientSide/clientRepos";
char DOT_UPDATE[] = ".Update";
char DOT_MANIFEST[] = ".Manifest";
char DOT_CONFLICT[] = ".Conflict";
char DOT_COMMIT[] = ".Commit";
long int id = 0;

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
	int clientManifest = open(pathToManifest, O_CREAT | O_WRONLY | O_TRUNC, 00777);
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

Manifest* readManifestInClientSide(char *projName) {
	char *path = malloc(sizeof(char) * (strlen(projName) + 50));

    char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(DOT_MANIFEST) )));
    sprintf(pathToManifest, "%s/%s/%s", CLIENT_REPOS, projName, DOT_MANIFEST);
	
	if(!fileExistsCheck(pathToManifest)) {
		free(pathToManifest);
		return NULL;
	}
	
	int manifestFd = open(pathToManifest, O_RDONLY, 0777);
	Manifest *manifest = readManifest(manifestFd);
	close(manifestFd);
	
	free(pathToManifest);
	return manifest;
}

void configure(char* hostName, char* port)
{
    remove("./.configure");
    int fd = open("./.configure", O_WRONLY | O_CREAT, 00600);
    write(fd, hostName, strlen(hostName));
    write(fd, "\n", 1);
    write(fd, port, strlen(port));
    printf("Configured on port %s.\n", port);
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
	if(DirExistsCheck(projName) == 0) {
		//printf("Error: The project does not exist.\n");
		return 0;
	}
	
	char *path = (char*)malloc(sizeof(char) * (strlen(projName) + strlen("/") + strlen(".Manifest")));
	
	// check if project directory contains manifest
	sprintf(path, "%s/%s", projName, ".Manifest");
	if(!fileExistsCheck(path)) {
		printf("Error: Please create project first.\n");
		free(path);
		return 0;
	}
	return 1;
}

void writeDotToSocket(int sock, char *projName, const char *fileExtension) {
	
	// Check first if project exists.
	printf("Writing File %s in project: %s to client.\n", fileExtension, projName);
	char *path = malloc(sizeof(char) * (strlen(projName) + strlen(fileExtension) + 50 + strlen(CLIENT_REPOS)));
	
	// Read current version of project.
	//char *version = readCurrentVersion(projName);

	// Create file path on server.
	sprintf(path, "%s/%s/%s", CLIENT_REPOS, projName, fileExtension);
	//free(version);
	
	long fileSize = findFileSize(path);	
    
    //Get the contents of the file		
	int fd = open(path, O_RDONLY, 00777);
    char* readIn = (char*)(malloc(sizeof(char) * fileSize));
    int numBytesRead = read(fd, readIn, fileSize);
    close(fd);

    char* fileInfo = (char*)(malloc(sizeof(char) * (16 + strlen(fileExtension) + 16)));
	sprintf(fileInfo, "%d:%s:%ld:", strlen(fileExtension), fileExtension, fileSize);
	write(sock, fileInfo, strlen(fileInfo));
    //printf("%s\n", readIn);
		
    //Write contents 
    write(sock, readIn, numBytesRead);
    //sendfile:1:9:.Manifest:<numBytesRead>:<readIn>

	free(path);
    free(readIn);
    free(fileInfo);
}


// failed: error:
void writeErrorToSocket(int sock, char *error) {
	write(sock, "failed:", strlen("failed:"));
	write(sock, error, strlen(error));
	write(sock, ":", 1);
}



void commit(char* projName)
{
    //check if project exists on client
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);
    if(projExistsInClient(pathToProject) == 0) {
        printf("Error: Project does not exist in local repo.\n");
        free(pathToProject);
        return;
    }

    //char* pathToUpdate = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(DOT_UPDATE))));
    char* pathToUpdate = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_UPDATE))));

    // check if update file is there and is not empty
	sprintf(pathToUpdate, "%s/%s/%s", CLIENT_REPOS, projName, DOT_UPDATE);
    //printf("%s\n\n",pathToUpdate);
	if(fileExistsCheck(pathToUpdate) && findFileSize(pathToUpdate) != 0) {
		printf("Error: Client has a .Update file that isn't empty.\n");
		free(pathToUpdate);
        free(pathToProject);
		return;
	}
    free(pathToUpdate);

    char* pathToConflict = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_CONFLICT))));
    //check if there is a .Conflict file
    sprintf(pathToConflict, "%s/%s/%s", CLIENT_REPOS, projName, DOT_CONFLICT);
    if(fileExistsCheck(pathToConflict))
    {
        printf("Error: Client has a .Conflict file.\n");
        free(pathToProject);
        free(pathToConflict);
        return;
    }
    free(pathToConflict);

    int sock, port, numBytes;
    struct sockaddr_in serverAddr;
    struct hostent* server;

    //read from .configure file to obtain IP and port#
    int fd = open("./.configure", O_RDONLY);
    if(fd == -1) { //if file does not exist
        printf("Error: missing .configure\n");
        free(pathToProject);
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
        free(pathToProject);
        return;
    }

    fd = open("./.configure", O_RDONLY);
    char hostName[256];
    getHostName(fd, hostName);
    server = gethostbyname(hostName);
    if(server == NULL)
    {
        printf("Error: no such host.\n");
        free(pathToProject);
        return;
    }

    bzero((char*)(&serverAddr), sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char*)(server->h_addr), (char*)(&serverAddr.sin_addr.s_addr), server->h_length);
    serverAddr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr*)(&serverAddr), sizeof(serverAddr)) < 0)
    {
        printf("Error: could not connect.\n");
        free(pathToProject);
        return;
    }

    printf("Getting .Manifest of Project: %s.\n", projName);

    // Step 1: Fetch server's .Manifest for specified project
    // Send socket message
    // <lengthAfterFirstColon>:com:<projectName>

    char* baseCmd = (char*)(malloc((strlen("com:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "com:");
    strcat(baseCmd, projName);
    strcat(baseCmd, "\0");
    //printf("%s\n\n", baseCmd);

    int numBytesToSend = strlen(baseCmd);
    char numBytesBuffer[10];
    memset(numBytesBuffer, '\0', 10 * sizeof(char));
    sprintf(numBytesBuffer, "%d", numBytesToSend);
    //printf("%s\n\n", numBytesBuffer);

    char* fullCmd = (char*)(malloc((strlen(numBytesBuffer) + 1 + strlen(baseCmd) + 1) * sizeof(char)));
    strcpy(fullCmd, numBytesBuffer);
    strcat(fullCmd, ":");
    strcat(fullCmd, baseCmd);
    strcat(fullCmd, "\0");
    //printf("%s\n\n", fullCmd);

    free(baseCmd);

    send(sock, fullCmd, strlen(fullCmd), 0);

    free(fullCmd);

    // server sends back 
    // sendfile:<numFiles>:<ManifestNameLen>:<manifest name>:<numBytesOfContent>:<contents>
	// OR "failed:<fail Reason>:"
 
    SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sock, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		
        //The client should output a list of all
        readTillDelimiter(socketBuffer, sock, ':');
        char* numFiles = readAllBuffer(socketBuffer);
        int nF = atoi(numFiles);

        readTillDelimiter(socketBuffer, sock, ':');
        char* lenExt = readAllBuffer(socketBuffer);
        long lExt = atol(lenExt);

        readNBytes(socketBuffer, sock, lExt + 1);
        char* ext = readAllBuffer(socketBuffer);
        
        readTillDelimiter(socketBuffer, sock, ':');
        char* numBytesContent = readAllBuffer(socketBuffer);
        long nBytesContent = atol(numBytesContent);
        
        Manifest* serverManifestList = readManifest(sock);

        readTillDelimiter(socketBuffer, sock, ':');
        char* ID = readAllBuffer(socketBuffer);
        id = atol(ID);
        char* pathToIDFile = (char*)(malloc(sizeof(char) * strlen("./.ID")));
        strcpy(pathToIDFile, "./.ID");
        int idFD = open(pathToIDFile, O_CREAT| O_WRONLY, 00777);
        write(idFD, ID, strlen(ID));
        //printf("%s\n", ID);

        Manifest* clientManifestList = readManifestInClientSide(projName);

        //Step 2: Check if version numbers of client and server .Manifest files match
        //if they do, create a .Commit file and read through client's .Manifest
            //read each entry that has a code that is NOT 'N'
            //. Look for those codes that signify added files, changed files and 
            //removed files rather than scanning through the entire entry). 
        //printf("%d\n", serverManifestList->versionNum);
        //printf("%d\n", clientManifestList->versionNum);
        if(serverManifestList->versionNum > clientManifestList->versionNum) 
        {
            writeErrorToSocket(sock, "Client has to update project first.");
            printf("Error: .Manifest version numbers do not match, call update on project first.\n");
            free(responseCode);
            freeSocketBuffer(socketBuffer);
            freeManifestEntryNodes(serverManifestList);
            //freeManifestEntryNodes(clientManifestList);
            free(serverManifestList);
            //free(clientManifestList);
            free(pathToProject);
            return;
        }
        //if they do, create a .Commit file and read through client's .Manifest
            //read each entry that has a code that is NOT 'N'
            //. Look for those codes that signify added files, changed files and 
            //removed files rather than scanning through the entire entry. 
        else
        {
            //loop through each entry, if code for entry is not N, search through serverManifestList
            //we use findNodeByFilePath to find the same entry, CHECK HASH AND VERSION NUM of server
            //if server's file's version number is greater, then FAIL
            //if A, write entry into commit EXACTLY AS IS
            //if R, write entry into commit (with D instead of R), everything else EXACTLY AS IS
            //if M, 
                //When writing to .Commit for modified code, write with an incremented file version number and live hash code
                //If server .Manifest contains different hash AND greater version number for a file, FAIL
            //build path to .Commit, open .Commit
            char* pathToCommit = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 +strlen(DOT_COMMIT) + strlen(ID))));
            sprintf(pathToCommit, "%s/%s/%s%s", CLIENT_REPOS, projName, DOT_COMMIT, ID);
            remove(pathToCommit);
            int commitFD = open(pathToCommit, O_CREAT | O_WRONLY, 00777);
            write(commitFD, ID, strlen(ID));
            write(commitFD, "\n", 1);

            printf("\n");
            //read through each ManifestEntryNode in clientManifestList
            ManifestEntryNode* curr = clientManifestList->head;
            char* liveHash;
            while(curr != NULL)
            {
                //if this curr has code N, move on to next node
                if(strcmp(curr->manifestCode, "N") == 0)
                {
                    curr = curr->next;
                    continue;
                }
                //if this curr has code A, write the entry into .Commit as is (new code that server has never seen before)
                //Write the following: A:<filePath>:<versionNum>:<fileHash>
                //Print the following: A <filePath>
                else if(strcmp(curr->manifestCode, "A") == 0)
                {
                    write(commitFD, "A:", 2);

                    write(commitFD, curr->filePath, strlen(curr->filePath));
                    write(commitFD, ":", 1);

                    write(commitFD, curr->versionNum, strlen(curr->versionNum));
                    write(commitFD, ":", 1);

                    write(commitFD, curr->fileHash, strlen(curr->fileHash));

                    write(commitFD, "\n", 1);
                    
                    printf("A %s\n", curr->filePath);
                }
                //if this curr has code R, write the entry into .Commit as is (code that needs to be deleted)
                else if(strcmp(curr->manifestCode, "R") == 0)
                {
                    write(commitFD, "D:", 2);

                    write(commitFD, curr->filePath, strlen(curr->filePath));
                    write(commitFD, ":", 1);

                    write(commitFD, curr->versionNum, strlen(curr->versionNum));
                    write(commitFD, ":", 1);

                    write(commitFD, curr->fileHash, strlen(curr->fileHash));

                    write(commitFD, "\n", 1);
                    
                    printf("D %s\n", curr->filePath);
                }
                //if this curr has code M, write the entry into .Commit with incremented file version and new hash
                //Write the following: M:<filePath>:<versionNum++>:<liveFileHash>
                //Print the following: M <filePath>
                else if(strcmp(curr->manifestCode, "M") == 0)
                {
                    //get live hash
                    char* pathToFile = curr->filePath;
                    liveHash = strdup(hashFile(pathToFile));

                    //now check the hash and versionNumber against that of the server's entryNode for the same file
                        //if live hash is the same as client hash in .Manifest, client didn't make real modifications, so ignore it
                        //if live hash is different, check the version number to make sure there is no file de-sync (if file's version number of server is greater, FAIL)
                    ManifestEntryNode* fileInServer = findNodeByFilePath(serverManifestList, curr->filePath);
                    //check if live hash is the same as client hash, if it is, increment curr and continue (treat as N)
                    if(strcmp(liveHash, curr->fileHash) == 0)
                    {
                        //not real modification, treat this as code N
                        curr = curr->next;
                        continue;
                    }
                    //otherwise, live hash is for sure different from hash in client's .Manifest for that file
                    else
                    {
                        //increment version number
                        int verNum = atoi(curr->versionNum);
                        verNum ++;
                        char vN[10];
                        memset(vN, '\0', 10 * sizeof(char));
                        sprintf(vN, "%d", verNum);
                        curr->versionNum = vN;

                        //check the version number of file in server against version number of file in client
                        if(atoi(fileInServer->versionNum) > atoi(curr->versionNum))
                        {
                            writeErrorToSocket(sock, "Client must synch with the repository before making changes.");
                            printf("Error: Client must synch with the repository before making changes.\n");
                            close(commitFD);
                            remove(pathToCommit);
                            free(pathToCommit);
                            free(pathToProject);
                            freeManifestEntryNodes(serverManifestList);
                            free(serverManifestList);
                            //freeManifestEntryNodes(clientManifestList);
                            free(clientManifestList);
                            return;
                        }
                        //otherwise, there is no file de-sync, so write it to .Commit as a modify code
                        //Write the following: M:<filePath>:<versionNum>:<fileHash>
                        //Print the following: M <filePath>
                        else
                        {
                            write(commitFD, "M:", 2);

                            write(commitFD, curr->filePath, strlen(curr->filePath));
                            write(commitFD, ":", 1);

                            write(commitFD, curr->versionNum, strlen(curr->versionNum));
                            write(commitFD, ":", 1);

                            write(commitFD, liveHash, strlen(liveHash));

                            write(commitFD, "\n", 1);
                            
                            printf("M %s\n", curr->filePath);
                        }
                    }
                }
                curr = curr->next;
            }
            printf("\n.Commit has been made.\n");
            close(commitFD);
            freeManifestEntryNodes(serverManifestList);
            free(serverManifestList);

            //Send the .Commit to the server

			// sendfile:<numFiles>:<projectNameLength>:<projectName><File1LenBytes>:<File1Contents>
            //char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));

            //build name for .Commit<ID>
            char* commitName = (char*)(malloc(sizeof(char) * (strlen(DOT_COMMIT) + strlen(ID))));
            sprintf(commitName, "%s%s", DOT_COMMIT, ID);

            // send commit file back
            write(sock, "sendfile:", strlen("sendfile:"));
            write(sock, "1:", 2);
            writeDotToSocket(sock, projName, commitName);
            free(pathToCommit);
            free(commitName);
        }
    }
    else
    {
        printf("Project could not be committed.\n");		
        readTillDelimiter(socketBuffer, sock, ':');
        char *reason = readAllBuffer(socketBuffer);
        printf("Reason: %s\n", reason);
        free(reason);
    }
    
}

void rollback(char* projName, char* version)
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

    // Request to get project from server
    //<lengthAfterFirstColon>:roll:<projName>:<version>
    char* baseCmd = (char*)(malloc((strlen("roll:") + strlen(projName) + 1 + strlen(version)) * sizeof(char)));
    strcpy(baseCmd, "roll:");
    strcat(baseCmd, projName);
    strcat(baseCmd, ":");
    strcat(baseCmd, version);
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

    send(sock, fullCmd, strlen(fullCmd), 0);
    free(baseCmd);

}

void upgrade(char* projName)
{

    //Build path to project
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);

    // Check if project exists
	if(projExistsInClient(pathToProject) == 0) {
		printf("Error: Project does not exist in local repo.\n");
        free(pathToProject);
		return;
	}

    //if there is no .Update on the client side or if .Conflict exists fail
    char* pathToDotUpdate = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(DOT_UPDATE))));
    sprintf(pathToDotUpdate, "%s/%s/%s", CLIENT_REPOS, projName,DOT_UPDATE);
    //printf("%s\n", pathToDotUpdate);
    if(fileExistsCheck(pathToDotUpdate)==0)   
    {
		printf("Error: Call update on project first.\n");
        free(pathToProject);
        free(pathToDotUpdate);
        return;
    } 

    if(fileExistsCheck(pathToDotUpdate) && findFileSize(pathToDotUpdate)==0)
    {
        printf("Up to Date, no need to upgrade.\n");
        remove(pathToDotUpdate);
        free(pathToProject);
        free(pathToDotUpdate);
        return;

    }

    char* pathToDotConflict = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(DOT_CONFLICT))));
    sprintf(pathToDotConflict, "%s/%s/%s", CLIENT_REPOS, projName,DOT_CONFLICT);
    //printf("%s\n", pathToDotConflict);

    if(fileExistsCheck(pathToDotConflict)!=0)   
    {
		printf("Error: Resolve the conflicts first and update project again.\n");
        free(pathToProject);
        free(pathToDotUpdate);
        free(pathToDotConflict);
        return;
    }

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

    //Step 3: Once connection is established, check to see if project exists on server
        //if it does not exist on server, server reports that project DNE, end command
        //if it DOES exist...
            //need a way to compress the entire project folder for the latest version
            //could use system(tar...) to compress whole folder on serverSide, then...
            //send that compressed file over to clientSide and decompress it
            //extract all files, rename the folder from <versionNumber> to <projectName>

    // Request to get project from server
    //<lengthAfterFirstColon>:co:<projName>
    char* baseCmd = (char*)(malloc((strlen("upg:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "upg:");
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

    send(sock, fullCmd, strlen(fullCmd), 0);
    free(baseCmd);

    //get client's current .Manifest into a struct
    char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + strlen("/.Manifest"))));
    sprintf(pathToManifest, "%s/%s/.Manifest", CLIENT_REPOS, projName);

    int manifestFD = open(pathToManifest, O_RDONLY, 00777);
    Manifest* clientManifest = readManifest(manifestFD);
    close(manifestFD);

    //receive version number from server
    SocketBuffer* socketBuffer = createBuffer();
    readTillDelimiter(socketBuffer, sock, ':');
    char* serverVersion = readAllBuffer(socketBuffer);
    int sVersion = atoi(serverVersion);
    //printf("%d\n", sVersion);
    clientManifest->versionNum = sVersion;

    //apply the changes in the .Update file
    // - need to loop reading of .Update file
    int updateFD = open(pathToDotUpdate, O_RDONLY, 00777);
    char* updateCode = (char*)(malloc(sizeof(char) * 3));
    memset(updateCode, '\0', 3 * sizeof(char));
    while(read(updateFD, updateCode, 2) == 2)
    {
        if(strcmp(updateCode, "D:") == 0)
        {
            //filePath
            readTillDelimiter(socketBuffer, updateFD, ':');
            char* fp = readAllBuffer(socketBuffer);
            clearSocketBuffer(socketBuffer);
            
            //fileVersion
            readTillDelimiter(socketBuffer, updateFD, ':');
            clearSocketBuffer(socketBuffer);

            //fileHash
            readTillDelimiter(socketBuffer, updateFD, '\n');
            clearSocketBuffer(socketBuffer);

            removeEntryFromManifest(clientManifest, fp);
            remove(fp);

            continue;           
        }
        
        //read filePath
        readTillDelimiter(socketBuffer, updateFD, ':');
        char* filePath = readAllBuffer(socketBuffer);
        //printf("%s\n", filePath);
        clearSocketBuffer(socketBuffer);

        //send this filePath to the server so that it can send its contents back to here
        //send(sock, filePath, strlen(filePath), 0);
        char filePathLen[11];
        memset(filePathLen, '\0', 11 * sizeof(char));
        sprintf(filePathLen, "%d:", strlen(filePath));
        //send this msg to server- 
        write(sock, "sendPath:", strlen("sendPath:"));
        write(sock, filePathLen, strlen(filePathLen));
        write(sock, filePath, strlen(filePath));

        //read in versionNum
        readTillDelimiter(socketBuffer, updateFD, ':');
        char* verNum = readAllBuffer(socketBuffer);
        //printf("%s\n", verNum);
        clearSocketBuffer(socketBuffer);

        //read in hashCode
        readTillDelimiter(socketBuffer, updateFD, '\n');
        char* fHash = readAllBuffer(socketBuffer);
        //printf("%s\n", fHash);
        clearSocketBuffer(socketBuffer);

        //if check for adding manifest node
        if(strcmp(updateCode, "A:") == 0)
        {
            //adding file, need to create new entry
            addEntryToManifest(clientManifest, filePath, verNum, fHash, "N");
        }

        //if check for if it's M
        else if(strcmp(updateCode, "M:") == 0)
        {
            ManifestEntryNode* modifiedEntry = findNodeByFilePath(clientManifest, filePath);
            if(modifiedEntry == NULL)
            {
                printf("oopsie\n");
            }
            else
            {
                //inc. ver. num.
                modifiedEntry->versionNum = verNum;

                //update file hash
                modifiedEntry->fileHash = fHash;
            
                //set manifestCode to N
                modifiedEntry->manifestCode = "N";
            }
        }
        //wait for client to send a message containing fileContents
        //responseCode
        //sendfile:
        readTillDelimiter(socketBuffer, sock, ':');
        char* sendFileMSG = readAllBuffer(socketBuffer);
        //printf("%s:", sendFileMSG);
        clearSocketBuffer(socketBuffer);

        //fileName
        readTillDelimiter(socketBuffer, sock, ':');
        char* fileName = readAllBuffer(socketBuffer);
        //printf("%s:", fileName);
        clearSocketBuffer(socketBuffer);

        //fileSize
        readTillDelimiter(socketBuffer, sock, ':');
        char* fileSize = readAllBuffer(socketBuffer);
        long fSize = atol(fileSize);
        //printf("%s:", fileSize);
        clearSocketBuffer(socketBuffer);

        //fileContent
        char* fileContent = (char*)(malloc(sizeof(char) * fSize));
        read(sock, fileContent, fSize);
        //printf("%s\n", fileContent);

        //start to write contents to file
        char* pathToUpdatedFile = (char*)(malloc(sizeof(char) * (strlen(pathToProject) + 1 + strlen(fileName))));
        sprintf(pathToUpdatedFile, "%s/%s", pathToProject, fileName);
        int fileFD = open(pathToUpdatedFile, O_CREAT | O_WRONLY, 00777);
        write(fileFD, fileContent, strlen(fileContent));
        close(fileFD);
    }
    close(updateFD);
    remove(pathToDotUpdate);

    //rewrite .Manifest using manifest struct
    manifestFD = open(pathToManifest, O_CREAT | O_WRONLY, 00777);
    writeToManifest(manifestFD, clientManifest);
    close(manifestFD);

    send(sock, "fin:", strlen("fin:"), 0);
    printf("Finished upgrading project.\n");
}


void update(char* projName)
{
     //Build path to project
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);

    // Check if project exists
	if(projExistsInClient(pathToProject) == 0) {
		printf("Error: Project does not exist in local repo.\n");
        free(pathToProject);
		return;
	}

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

    // Request server for .Manifest for project
	// upd:<projName>
   //<lengthAfterFirstColon>:cv:<projectName>
    char* baseCmd = (char*)(malloc((strlen("upd:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "upd:");
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

    // server sends back 
    // sendfile:<numFiles>:<ManifestNameLen>:<manifest name>:<numBytesOfContent>:<contents>
	// OR "failed:<fail Reason>:"
 
    SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sock, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		
        //The client should output a list of all
        //files under the project name, along with their version number (i.e., number of updates).
        readTillDelimiter(socketBuffer, sock, ':');
        char* numFiles = readAllBuffer(socketBuffer);
        int nF = atoi(numFiles);

        readTillDelimiter(socketBuffer, sock, ':');
        char* lenExt = readAllBuffer(socketBuffer);
        long lExt = atol(lenExt);

        readNBytes(socketBuffer, sock, lExt + 1);
        char* ext = readAllBuffer(socketBuffer);
        
        readTillDelimiter(socketBuffer, sock, ':');
        char* numBytesContent = readAllBuffer(socketBuffer);
        long nBytesContent = atol(numBytesContent);
        
        // LL of server manifest
        Manifest* serverManifestList = readManifest(sock);

        // Now get local manifest
        char* pathToLocManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + 1 + strlen(DOT_MANIFEST))));
        sprintf(pathToLocManifest, "%s/%s/%s", CLIENT_REPOS, projName, DOT_MANIFEST);

        int locManifestFd = open(pathToLocManifest, O_RDONLY, 00777);
        Manifest* localManifestList = readManifest(locManifestFd);
        close(locManifestFd);

        char* pathToUpdate = (char*)(malloc(sizeof(char) * (strlen(pathToProject) + 1 + strlen(DOT_UPDATE))));
        sprintf(pathToUpdate, "%s/%s", pathToProject, DOT_UPDATE);
        int updateFD = open(pathToUpdate, O_CREAT | O_WRONLY, 00777);

        if(localManifestList->versionNum == serverManifestList->versionNum)
        {
            printf("Up to Date.\n");
            close(updateFD);
            char* pathToConflict = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_CONFLICT))));
            remove(pathToConflict);
            return;
        }

        // Compare serverManifestList and localManifestList
        compareManifests(serverManifestList, localManifestList, pathToProject, updateFD);
    }
    else {
        // failed to update
        printf("Project could not be updated.\n");		
        readTillDelimiter(socketBuffer, sock, ':');
        char *reason = readAllBuffer(socketBuffer);
        printf("Reason: %s\n", reason);
        free(reason);
	}
}

void push(char* projName)
{
    //Build path to project
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);

    // Check if project exists
	if(projExistsInClient(pathToProject) == 0) {
		printf("Error: Project does not exist in local repo.\n");
        free(pathToProject);
		return;
	}
    //Check if commit exists
    else
    {
        char ID[11];
        memset(ID, '\0', 11 * sizeof(char));
        sprintf(ID, "%ld", id);
        char* pathToCommit = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 +strlen(DOT_COMMIT) + strlen(ID))));
        sprintf(pathToCommit, "%s/%s/%s%s", CLIENT_REPOS, projName, DOT_COMMIT, ID);
        //printf("%s\n", pathToCommit);
        int fd = open(pathToCommit, O_RDONLY, 00777);
        if(fd == -1) { //if file does not exist
            printf("Error: Missing .Commit file.\n");
            close(fd);
            return;
        }
        close(fd);
        free(pathToCommit);
        free(pathToProject);
    }
    
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

    //printf("Sending Project: %s to destroy\n", projName);

    // <lenCMD>:<push>:<projName>
    char* baseCmd = (char*)(malloc((strlen("push:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "push:");
    strcat(baseCmd, ":");
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
    //printf("%s\n", fullCmd);

    free(baseCmd);
    
    send(sock, fullCmd, strlen(fullCmd), 0);
    free(fullCmd);

    char ID[11];
    memset(ID, '\0', 11 * sizeof(char));
    sprintf(ID, "%ld:", id);
    send(sock, ID, strlen(ID), 0);

    char commitStatusMessage[100];
    memset(commitStatusMessage, '\0', 50 * sizeof(char));
    read(sock, commitStatusMessage, 50 * sizeof(char));
    printf("%s", commitStatusMessage);

    SocketBuffer* socketBuffer = createBuffer();
    int loopEnabled = 1;
    while(loopEnabled)
    {
        readTillDelimiter(socketBuffer, sock, ':');
        char* response = readAllBuffer(socketBuffer);
        if(strcmp(response, "fin") == 0) {
            loopEnabled = 0;
            /*wait to receive updated .Manifest here*/
            //1:9:.Manifest:<numBytesRead>:<readIn>
            //numFiles
            readTillDelimiter(socketBuffer, sock, ':');
            char* nF = readAllBuffer(socketBuffer);
            //printf("%s:", nF);
            clearSocketBuffer(socketBuffer);
            
            //length of .Manifest
            readTillDelimiter(socketBuffer, sock, ':');
            char* lenManifest = readAllBuffer(socketBuffer);
            //printf("%s:", lenManifest);
            clearSocketBuffer(socketBuffer);

            //".Manifest"
            readTillDelimiter(socketBuffer, sock, ':');
            char* manifestName = readAllBuffer(socketBuffer);
            //printf("%s:", manifestName);
            clearSocketBuffer(socketBuffer);

            //numBytesOfContent
            readTillDelimiter(socketBuffer, sock, ':');
            char* numBytesOfContent = readAllBuffer(socketBuffer);
            //printf("%s:", numBytesOfContent);
            long nBytesContent = atol(numBytesOfContent);
            clearSocketBuffer(socketBuffer);

            //content
            //char* content = (char*)(malloc(sizeof(char) * nBytesContent));
            //read(sock, content, nBytesContent);
            readNBytes(socketBuffer, sock, nBytesContent);
            char* content = readAllBuffer(socketBuffer);
            //printf("%s", content);
            clearSocketBuffer(socketBuffer);

            //build path to .Manifest on client, create/write .Manifest on client
            char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + strlen("/.Manifest"))));
            sprintf(pathToManifest, "%s/%s/.Manifest", CLIENT_REPOS, projName);
            //printf("%s\n", pathToManifest);
            int manifestFD = open(pathToManifest, O_CREAT | O_WRONLY, 00777);
            write(manifestFD, content, nBytesContent);
            close(manifestFD);

            break;
        }
        else
        {
            clearSocketBuffer(socketBuffer);
            readTillDelimiter(socketBuffer, sock, ':');
            char* lenFilePath = readAllBuffer(socketBuffer);
            int lFilePath = atoi(lenFilePath);

            clearSocketBuffer(socketBuffer);
            readNBytes(socketBuffer, sock, lFilePath);
            char* filePath = readAllBuffer(socketBuffer);
            //printf("%s\n", filePath);
            
            clearSocketBuffer(socketBuffer);

            //once we have a filePath, open the file specified by filePath, send contents back as a message
            char* fileName = strrchr(filePath, '/') + 1;
            long int fileSize = findFileSize(filePath);
            char fileSizeBuf[11];
            memset(fileSizeBuf, '\0', sizeof(char) * 11);
            sprintf(fileSizeBuf, "%ld", fileSize);
            
            int fileFD = open(filePath, O_RDONLY, 00777);
            char* fileContent = (char*)(malloc(sizeof(char) * fileSize));
            read(fileFD, fileContent, fileSize);
            close(fileFD);
            
            char* msg = (char*)(malloc(sizeof(char) * (strlen("sendfile:") + strlen(fileName) + 1 + strlen(fileSizeBuf) + 1 + strlen(fileContent))));
            sprintf(msg, "sendfile:%s:%s:%s", fileName, fileSizeBuf, fileContent);
            //printf("%s\n", msg);

            send(sock, msg, strlen(msg), 0);

            //char* fileMSG = (char*)(malloc(sizeof(char) * (strlen("sendfile:") + strlen(fileName) + 1 + strlen(fileSizeBuf) + 1 + strlen(fileContent))));
            //sprintf(fileMSG, "sendfile:%s:%s:%s", fileName, fileSizeBuf, fileContent);
            //printf("%s\n", fileMSG);
        }
    }

    char finMSG[5];
    memset(finMSG, '\0', 5 * sizeof(char));
    read(sock, finMSG, 4);
    printf("Project successfully pushed.\n");
}

// add an entry for the the file
// to its client's .Manifest with a new version number and hashcode
void addFile(char* projName, char* fileName)
{
    //Build path to project
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);

    // Check if project exists
	if(projExistsInClient(pathToProject) == 0) {
		printf("Error: Project does not exist in local repo.\n");
		return;
	}

    //Build path to file
    char *pathToFile = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(fileName))));
    sprintf(pathToFile, "%s/%s/%s", CLIENT_REPOS, projName, fileName);

    // check if given file exists
	if(!fileExistsCheck(pathToFile)) {
		printf("Error: File does not exist locally in project.\n");
		printf("Cannot to find: %s\n", pathToFile);
		return;
	}

    // check if project directory contains manifest
    //.Manifest:
    /*
    (A) (M) (R)
    Ex:
    1
    <filePath>:<verNum>:<hash>:<code>
    //./clientRepos/test10/test.txt
    //./clientRepos/test10/yerrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr.txt

    */
    //Start to read through .Manifest
    char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(".Manifest"))));
    sprintf(pathToManifest, "%s/%s/%s", CLIENT_REPOS, projName, ".Manifest");
    //Open Manifest
    int manifestFD = open(pathToManifest, O_RDONLY, 00777);

    //Read manifest into Manifest struct
    Manifest* manifestList = readManifest(manifestFD);
    close(manifestFD);
    //printf("Version: %d\nNumber of Files: %d\n", manifestList->versionNum, manifestList->numFiles);
    
    char* hash;
    //Check numFiles of manifestList, if 0, that means there are no entries (or no files in the project)
    if(manifestList->numFiles == 0)
    {
        //printf("no files\n");
        //hash file, ensure you have all attributes necessary, add entry into manifestList
        hash = strdup(hashFile(pathToFile));
    
        addEntryToManifest(manifestList, pathToFile, "1", hash, "A");
        printf("File '%s' has been added.\n", fileName);
    }
    else
    {
        //Find the entry with the matching file path
        ManifestEntryNode* entry = findNodeByFilePath(manifestList, pathToFile);

        //If entry is NULL, that means no entry was found
        if(entry == NULL)
        {
            //hash file, ensure you have all attributes necessary, add entry into manifestList
            hash = strdup(hashFile(pathToFile));

            //add to manifestList
            addEntryToManifest(manifestList, pathToFile, "1", hash, "A");
            printf("File '%s' has been added.\n", fileName);

        }
        else
        {
            //check entry's manifestCode:
            //if A, then need to change A to M, then rewrite
            if(strcmp(entry->manifestCode, "A") == 0)
            {
                entry->fileHash = strdup(hashFile(pathToFile));
                printf("File '%s' is already in .Manifest.\n", fileName);
            }
            //if R, then need to change R to A, since we are readding the file to the project
            else if(strcmp(entry->manifestCode, "R") == 0)
            {
                entry->manifestCode = "A";
                printf("File '%s' has been added.\n", fileName);
            }
            //if N (neutral code for when no code is necessary)
            else if(strcmp(entry->manifestCode, "N") == 0)
            {
                entry->manifestCode = "M";
                printf("File '%s' has been added.\n", fileName);
            }
            //if M, no need to make any changes
            else if(strcmp(entry->manifestCode, "M") == 0)
            {
                //nothing should happen, should just return out of the method
                //display that file is added
                printf("File '%s' has been added.\n", fileName);
                return;
            }
        }
    }

    //overwrite .Manifest to reflect changes made to an entry
    remove(pathToManifest);
    manifestFD = open(pathToManifest, O_CREAT | O_WRONLY, 00777);
    writeToManifest(manifestFD, manifestList);
    close(manifestFD);
}

void removeFile(char* projName, char* fileName)
{
    //check if project exists on client
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);
    if(projExistsInClient(pathToProject) == 0) {
        printf("Error: Project does not exist in local repo.\n");
        return;
    }

    //Start to read through .Manifest
    char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(".Manifest"))));
    sprintf(pathToManifest, "%s/%s/%s", CLIENT_REPOS, projName, ".Manifest");
    int manifestFD = open(pathToManifest, O_RDONLY, 00777);
    Manifest* manifestList = readManifest(manifestFD);
    close(manifestFD);
    
    //check if .Manifest is empty, if it is, nothing to remove
    if(manifestList->numFiles == 0)
    {
        printf("Error: .Manifest is empty.\n");
        return;
    }
    else
    {
        //Find the entry for the file
        char *pathToFile = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName) + strlen("/") + strlen(fileName))));
        sprintf(pathToFile, "%s/%s/%s", CLIENT_REPOS, projName, fileName);
        ManifestEntryNode* entry = findNodeByFilePath(manifestList, pathToFile);
        if(entry == NULL) //if there's no such match, then there's nothing to remove
        {
            printf("%s\n", pathToFile);
            printf("Error: file is not in .Manifest.\n");
            return;
        }
        else if(strcmp(entry->manifestCode, "R") == 0)
        {
            printf("Error: File '%s' has already been removed.\n");
            return;
        }
        else
        {
            entry->manifestCode = "R";
            printf("File '%s' has been removed.\n", fileName);
        }
    }

    //overwrite .Manifest to reflect changes made to an entry
    remove(pathToManifest);
    manifestFD = open(pathToManifest, O_CREAT | O_WRONLY, 00777);
    writeToManifest(manifestFD, manifestList);
    close(manifestFD);
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
    free(fullCmd);
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


void checkout(char* projName)
{
    //Step 1: Check if project exists locally
        //if it does, then end the command, don't even contact server
    char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + strlen("/") + strlen(projName))));
    sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);
    if(projExistsInClient(pathToProject) != 0) {
        printf("Error: Project exists in local repo.\n");
        free(pathToProject);
        return;
    }

    //Step 2: Check if you can connect to server
        //if no .configure, end command
        //if connection isn't valid (i.e. can't connect), end command

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

    //Step 3: Once connection is established, check to see if project exists on server
        //if it does not exist on server, server reports that project DNE, end command
        //if it DOES exist...
            //need a way to compress the entire project folder for the latest version
            //could use system(tar...) to compress whole folder on serverSide, then...
            //send that compressed file over to clientSide and decompress it
            //extract all files, rename the folder from <versionNumber> to <projectName>

    // Request to get project from server
    //<lengthAfterFirstColon>:co:<projName>
    char* baseCmd = (char*)(malloc((strlen("co:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "co:");
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

    send(sock, fullCmd, strlen(fullCmd), 0);
    free(baseCmd);

    //sendProj:<lenName>:<name>:<sizeOfManifest>:<content>
    //read manifestContent into Manifest struct
    //write .Manifest file on clientSide using Manifest struct
    SocketBuffer* socketBuffer = createBuffer();
    readTillDelimiter(socketBuffer, sock, ':');
    char* responseCode = readAllBuffer(socketBuffer);
    clearSocketBuffer(socketBuffer);
    if(strcmp(responseCode, "sendProj") == 0)
    {
        readTillDelimiter(socketBuffer, sock, ':');
        char* lenName = readAllBuffer(socketBuffer);
        //printf("%s:", lenName);
        clearSocketBuffer(socketBuffer);

        readTillDelimiter(socketBuffer, sock, ':');
        char* manifestName = readAllBuffer(socketBuffer);
        //printf("%s:", manifestName);
        clearSocketBuffer(socketBuffer);

        readTillDelimiter(socketBuffer, sock, ':');
        char* manifestSize = readAllBuffer(socketBuffer);
        long mSize = atol(manifestSize);
        //printf("%s:", manifestSize);
        clearSocketBuffer(socketBuffer);
        
        readNBytes(socketBuffer, sock, mSize);
        char* content = readAllBuffer(socketBuffer);
        //printf("%s", content);
        clearSocketBuffer(socketBuffer);

        //save .Manifest to local project
        char* pathToProject = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName))));
        sprintf(pathToProject, "%s/%s", CLIENT_REPOS, projName);
        //printf("%s\n", pathToProject);
        mkdir(pathToProject, 00777);

        char* pathToManifest = (char*)(malloc(sizeof(char) * (strlen(pathToProject) + strlen("/.Manifest"))));
        sprintf(pathToManifest, "%s/.Manifest", pathToProject);
        
        int manifestFD = open(pathToManifest, O_CREAT | O_WRONLY, 00777);
        write(manifestFD, content, mSize);
        close(manifestFD);

        //loop until server sends fin message
        int loopEnabled = 1;
        while(loopEnabled)
        {
            readTillDelimiter(socketBuffer, sock, ':');
            char* response = readAllBuffer(socketBuffer);
            clearSocketBuffer(socketBuffer);
            if(strcmp(response, "fin") == 0)
            {
                loopEnabled = 0;
                break;
            }
            else
            {
                readTillDelimiter(socketBuffer, sock, ':');
                char* fileName = readAllBuffer(socketBuffer);
                //printf("%s:", fileName);
                clearSocketBuffer(socketBuffer);

                readTillDelimiter(socketBuffer, sock, ':');
                char* fileSize = readAllBuffer(socketBuffer);
                long fSize = atol(fileSize);
                //printf("%s:", fileSize);
                clearSocketBuffer(socketBuffer);

                readNBytes(socketBuffer, sock, fSize);
                char* fileContent = readAllBuffer(socketBuffer);
                //printf("%s\n", fileContent);
                clearSocketBuffer(socketBuffer);

                //build path to file on client side
                char* pathToFile = (char*)(malloc(sizeof(char) * (strlen(CLIENT_REPOS) + 1 + strlen(projName) + 1 + strlen(fileName))));
                sprintf(pathToFile, "%s/%s/%s", CLIENT_REPOS, projName, fileName);
                //printf("%s\n", pathToFile);

                //open this built path to write into it
                int fileFD = open(pathToFile, O_CREAT | O_WRONLY, 00777);
                write(fileFD, fileContent, fSize);
                close(fileFD);

            }
        }
        printf("Project '%s' successfully created.\n", projName);
    }
    else
    {
        printf("Project could not be checked out from server.\n");		
        readTillDelimiter(socketBuffer, sock, ':');
        char *reason = readAllBuffer(socketBuffer);
        printf("Reason: %s\n", reason);
        free(reason);        
    }
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

    //<lengthAfterFirstColon>:cv:<projectName>
    char* baseCmd = (char*)(malloc((strlen("cv:") + strlen(projName) + 1) * sizeof(char)));
    strcpy(baseCmd, "cv:");
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

    // server sends back 
    // sendfile:<numFiles>:<ManifestNameLen>:<manifest name>:<numBytesOfContent>:<contents>
	// OR "failed:<fail Reason>:"
 
    SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sock, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		
        //The client should output a list of all
        //files under the project name, along with their version number (i.e., number of updates).
        readTillDelimiter(socketBuffer, sock, ':');
        char* numFiles = readAllBuffer(socketBuffer);
        int nF = atoi(numFiles);

        readTillDelimiter(socketBuffer, sock, ':');
        char* lenExt = readAllBuffer(socketBuffer);
        long lExt = atol(lenExt);

        readNBytes(socketBuffer, sock, lExt + 1);
        char* ext = readAllBuffer(socketBuffer);
        
        readTillDelimiter(socketBuffer, sock, ':');
        char* numBytesContent = readAllBuffer(socketBuffer);
        long nBytesContent = atol(numBytesContent);
        
        Manifest* manifestList = readManifest(sock);
        ManifestEntryNode* curr = manifestList->head;
        while(curr != NULL)
        {
            //printf("%s\n", curr->filePath);
            char* fileName = strrchr(curr->filePath, '/') + 1;
            printf("File: %s\t\tVersion: %s\n", fileName, curr->versionNum);
            curr = curr->next;
        }
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
    free(fullCmd);

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
    free(responsefrmServer);
	freeSocketBuffer(socketBuffer);

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
    //check for ID and save it
    int idFD = open("./.ID", O_RDONLY, 00777);
    if(idFD == -1)
    {
        close(idFD);
    }
    else
    {
        char numID[11];
        memset(numID, '\0', sizeof(char) * 11);
        read(idFD, numID, 1);
        id = atol(numID);
        //printf("ID: %ld\n", id);
    }
    

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
    else if(strcmp(argv[1], "currentversion") == 0)
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
        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            destroy(argv[2]);
		}
    }
    //add step
    else if(strcmp(argv[1], "add") == 0)
    {
         if(argc < 4) {
			printf("Error: Parameters missing\n");
		} else {
            addFile(argv[2], argv[3]);
		}
        
    }
    else if(strcmp(argv[1], "remove") == 0)
    {
        if(argc < 4) {
			printf("Error: Parameters missing\n");
		} else {
            removeFile(argv[2], argv[3]);
		}
    }
     else if(strcmp(argv[1], "update") == 0)
    {
        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            update(argv[2]);
		}
    }
    else if(strcmp(argv[1], "checkout") == 0)
    {
        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            checkout(argv[2]);
		}
        
        //CHECKOUT
        //Step 1: Check if project exists locally
            //if it does, then end the command, don't even contact server
        //Step 2: Check if you can connect to server
            //if no .configure, end command
            //if connection isn't valid (i.e. can't connect), end command
        //Step 3: Once connection is established, check to see if project exists on server
            //if it does not exist on server, server reports that project DNE, end command
            //if it DOES exist...
                //need a way to compress the entire project folder for the latest version
                //could use system(tar...) to compress whole folder on serverSide, then...
                //send that compressed file over to clientSide and decompress it
                //extract all files, rename the folder from <versionNumber> to <projectName>
    }
    else if(strcmp(argv[1], "commit") == 0)
    {

        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            commit(argv[2]);
		}

        //COMMIT
        //Step 1: Fetch server's .Manifest for specified project
        //Step 2: Check if version numbers of client and server .Manifest files match
            //if they do, create a .Commit file and read through client's .Manifest
                //read each entry that has a code that is NOT 'N'
                //. Look for those codes that signify added files, changed files and 
                //removed files rather than scanning through the entire entry). 
        //Step 3: writing to .Commit file
            //Where code is 'A,' that means file must be added, write to .Commit AND stdout: "A <file path>"
            //Where code is 'M,' that means file must be modified, write to .Commit AND stdout: "M <file path>"
            //Where code is 'R,' that means file must be removed, write to .Commit AND stdout: "D <file path>"
            //Add: client will have a file not in server
            //Modify: client and server will have file, but hash of client will be different from live hash
            //Delete: client will untrack a file, server will still have it
            //When writing to .Commit for modified code, write with an incremented file version number and new hash code
        //Step 4: Send .Commit from client to server, server should save and report success back to client
        //if project not in server, FAIL
        //if client can't connect, FAIL
        //if client has a .Update that is not empty, FAIL (if there is no .Update, it's fine)
        //If .Manifest of client and server do not have matching version numbers, FAIL
        //If server .Manifest contains different hash AND greater version number for a file, FAIL
            //this means you have to sync with server repo first before committing any changes
        //If FAIL, delete .Commit
    }
    else if(strcmp(argv[1], "push") == 0)
    {

        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            push(argv[2]);
		}


        //server builds this message to send to client
        //request:<numFilesNeededByServer>:<lengthFileOnePath>:<fileOnePath>:<lengthFileTwoPath>:<fileTwoPath>:...:<lengthFileNPath>:<fileNPath>
        
        //client builds this message to send to server
        //sendFiles:<numFilesNeededByServer>:<lengthOfFirstFilePath>:<firstFilePath>:<sizeOfFirstFile>:<contentOfFirstFile>:...
    
        //PUSH
        //Step 1: Client send .Commit to server
            //if id is 0, client never called commit, so automatic FAIL
            //.Commit<ID>
                //if server can't open filename: .Commit<ID>, then FAIL
            //if server has .Commit for this client, and they are same, then server must request files
            //that need to be changed
            //If there are other .Commit files pending, expire them so that push calls from other clients fail
        //Step 2: If the check from step 1 passes, then server should send to client what files it needs
            //...unless they're being removed
                //readManifest();, need this so that you can create a new .Manifest for the new version
                //if an entry of the .Commit starts with D, remove it from the .Manifest
                    //whenever there's a D, there's also a filePath in that entry; search for that filePath in Manifest* struct, remove the corresponding node
                    //as soon as an entry in .Commit gets read through, the code for it in the Manifest* struct has to change from A/M to N (CHANGE manifestCode in ManifestEntryNode)
                //in .history, keep a list of all operations performed for each version of the project
                    //as you're reading each entry in .Commit, write the corresponding operation (A/M/D <fileName>) to .history (write under the versionNumber of the project during this push)
                    //ex. .history format:
                        /*
                        1
                        A <file1>
                        M <file2>
                        D <file3>
                        D <file4>
                        2
                        A<file5>

                        */
            //involves building the msg stated up top
        //Step 3: Client should then send those specified files
            //involves reading from the msg stated up top
            //client builds a msg to send to the server, containing number of files, file contents, etc.
        //Step 4: when server receives files, it should physically update files (create the ones it needs, replace the ones modified)
            //create directory for new version
            //for each file (specified by numFilesNeededByServer), call open() for each of them, then write the content for each of them
            //need to rewrite .Manifest for the new version
            //in project directory, increment version number in .version
        //Step 5: for both .Manifest files of client and server
            //...increment project version number
            //...increment file version numbers
            //...update hashes
            //...remove status codes
                //.Manifest of client and server must have:
                    /*incrementedVersionNumber
                      numFiles
                      <entry>
                      ...
                    */
                //best way: rewrite the .Manifest for the server, then send it to the client, client will then replace its current .Manifest with the one that server sends
                //use ManifestList!!
        //Step 6: expire the .Commit
            //close(commitFD);
            //remove(.Commit<ID>);
        //if project doesn't exist on server, FAIL
        //if client can't connect, FAIL
        //if the .Commit does not exist on the server, FAIL
        //if the .Commit files are not the same, FAIL (client has to call commit again)
    }

    else if(strcmp(argv[1], "upgrade") == 0)
    {

        if(argc < 3) {
			printf("Error: Parameters missing\n");
		} else {
            upgrade(argv[2]);
		}
    }

    else if(strcmp(argv[1], "rollback") == 0)
    {

        if(argc < 4) {
			printf("Error: Parameters missing\n");
		} else {
            rollback(argv[2], argv[3]);
		}
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
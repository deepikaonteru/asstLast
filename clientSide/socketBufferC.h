#ifndef SOCKET_BUFFER_H
#define SOCKET_BUFFER_H

#include <stdio.h>
#include <stdlib.h>

typedef struct ManifestEntryNode {
	char* filePath;
	char* versionNum;
	char* fileHash;
	char* manifestCode;
	struct ManifestEntryNode* next;
} ManifestEntryNode;

typedef struct Manifest {
	int versionNum;
	int numFiles;
	ManifestEntryNode *head;
	ManifestEntryNode *tail;
} Manifest;
	
typedef struct SocketNode {
	char c;
	struct SocketNode *next;
} SocketNode;

typedef struct SocketBuffer {
	int size;
	SocketNode *head;
	SocketNode *tail;
} SocketBuffer;

static char* hashFile(char* pathToFile)
{
	long sizeOfFile = 0;
	struct stat buffer;
	int status = stat(pathToFile, &buffer);
	// if permission available
	if(status == 0) {
		sizeOfFile = buffer.st_size;
	}
    //printf("%ld\n", sizeOfFile);

    //initialize a buffer, set it equal to file content
    char* contentBuffer = (char*)(malloc(sizeof(char) * sizeOfFile));
    memset(contentBuffer, '\0', sizeOfFile);
    int fileFD = open(pathToFile, O_RDONLY, 00777);
    int bytesReadFromFile = read(fileFD, contentBuffer, sizeof(char) * sizeOfFile);
    close(fileFD);
    //printf("%s\n", contentBuffer);

    //hash this buffer
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5(contentBuffer, strlen(contentBuffer), hash);
    /*
    int i;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", hash[i]);
    printf("\n");*/
    free(contentBuffer);

    //convert hash into hex format to save to manifestEntryNode
    char* strHash = (char*)(malloc(sizeof(char) * (MD5_DIGEST_LENGTH * 2 + 1)));
    int i;
    for(i = 0; i < MD5_DIGEST_LENGTH; i ++)
    {
        sprintf(strHash + 2 * i, "%02x", hash[i]);
    }
    //printf("%s\n", strHash);

    return strHash;
}

static void addCharToBuffer(SocketBuffer *socketBuffer, char c) {
	SocketNode *node = malloc(sizeof(SocketNode));
	node->c = c;
	node->next = NULL;
	
	if(socketBuffer->tail == NULL) {
		socketBuffer->tail = node;
		socketBuffer->head = node;
	} else {
		socketBuffer->tail->next = node;
		socketBuffer->tail = node;
	}
	socketBuffer->size += 1;
}

static SocketBuffer *createBuffer() {
	SocketBuffer *socketBuffer = malloc(sizeof(SocketBuffer));
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	return socketBuffer;
}

// this function returns a string
static char* readAllBuffer(SocketBuffer *socketBuffer) {
	char *result = malloc(sizeof(char) * (socketBuffer->size + 1));
	SocketNode *node = socketBuffer->head;
	
	int i = 0;
	while(node != NULL) {
		result[i++] = node->c;
		SocketNode *d = node;
		node = node->next;
		free(d);
	}
	// Add null terminator at last
	result[i] = '\0'; 
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	return result;
}

static void readNBytes(SocketBuffer *socketBuffer, int sockfd, long int numBytes) {
	char c;
	long int i = 0;
	while(i++ < numBytes) {
		read(sockfd, &c, 1);
		if(c == '\0') {			
			break; 
		}
		addCharToBuffer(socketBuffer, c);
	}
}

static void readTillDelimiter(SocketBuffer *socketBuffer, int sockfd, char delimiter) {
	char c;
	while(1) {
		read(sockfd, &c, 1);
		
		// for files,if EOF is reached, we get \0
		if(c == '\0') {
			break; 
		}
		
		if(c == delimiter) {
			break;
		}
		
		// Do not read the delimiter to buffer
		addCharToBuffer(socketBuffer, c);
	}
}

static void clearSocketBuffer(SocketBuffer *socketBuffer) {
	SocketNode *node = socketBuffer->head;

	while(node != NULL) {
		SocketNode *d = node;
		node = node->next;
		free(d);
	}
	
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	// Do not free the buffer object
}

static void freeSocketBuffer(SocketBuffer *socketBuffer) {
	clearSocketBuffer(socketBuffer);
	free(socketBuffer);
}

static void addEntryToManifest(Manifest *manifestList, char *filePath, char *versionNum, char *fileHash, char *manifestCode) {
	
	ManifestEntryNode *node = malloc(sizeof(ManifestEntryNode));
	node->filePath = filePath;
	node->versionNum = versionNum;
	node->fileHash = fileHash;
	node->manifestCode = manifestCode;
	node->next = NULL;
	
	if(manifestList->tail == NULL) {
		manifestList->tail = node;
		manifestList->head = node;
	} else {
		manifestList->tail->next = node;
		manifestList->tail = node;
	}
	
	manifestList->numFiles += 1;
}

static void removeEntryFromManifest(Manifest* manifestList, char* filePath)
{
	ManifestEntryNode* curr = manifestList->head;
	ManifestEntryNode* prev;
	if(curr != NULL && strcmp(manifestList->head->filePath, filePath) == 0)
	{
		manifestList->head = manifestList->head->next;
		free(curr);
		return;
	}
	while(curr != NULL && strcmp(curr->filePath, filePath) != 0)
	{
		prev = curr;
		curr = curr->next;
	}
	if(curr == NULL)
	{
		return;
	}
	prev->next = curr->next;
	free(curr);
}

//Ex:
    //1
	//<numFiles>
    //<filePath>:<verNum>:<hash>:<code>

static Manifest *readManifest(int fd) {
	
	SocketBuffer *socketBuffer = createBuffer();
	
	Manifest *manifestList = malloc(sizeof(Manifest));
	manifestList->head = NULL;
	manifestList->tail = NULL;
	manifestList->numFiles = 0;

	// first line is versionNumber
	readTillDelimiter(socketBuffer, fd, '\n');
	manifestList->versionNum = atoi(readAllBuffer(socketBuffer));

	// next line in manifest is numFiles
	readTillDelimiter(socketBuffer, fd, '\n');
	char *nF = readAllBuffer(socketBuffer);
	int numFiles = atoi(nF);
	free(nF); // free the string given by buffer.

	// now read n file entries
	int i = 0;
	while(i++ < numFiles) {
		char *filePath, *versionNum, *fileHash, *manifestCode;
		
		readTillDelimiter(socketBuffer, fd, ':');
		filePath = readAllBuffer(socketBuffer);
		//printf("%s\n", filePath);
		
		readTillDelimiter(socketBuffer, fd, ':');
		versionNum = readAllBuffer(socketBuffer);
		//printf("%s\n", versionNum);

		readTillDelimiter(socketBuffer, fd, ':');
		fileHash = readAllBuffer(socketBuffer);
		//printf("%s\n", fileHash);
		
		readTillDelimiter(socketBuffer, fd, '\n');
		manifestCode = readAllBuffer(socketBuffer);
		//printf("%s\n", manifestCode);

		addEntryToManifest(manifestList, filePath, versionNum, fileHash, manifestCode);
	}
	
	// free buffer
	freeSocketBuffer(socketBuffer);
	return manifestList;

}

static ManifestEntryNode* findNodeByFilePath(Manifest* manifestList, char* filePath)
{
	//Initialize temp pointer to head of list
	ManifestEntryNode* curr = manifestList->head;

	//As long as curr is NOT NULL, iterate through the list, see if filePath 
	//of node matches with filePath of function
	//return the node matches filePath
	//if match for curr not found, increment curr
	while(curr != NULL)
	{
		if(strcmp(curr->filePath, filePath) == 0)
		{
			return curr;
		}
		else
		{
			curr = curr->next;
		}
	}

	//if match was never found, return NULL
	return NULL;
}

static void writeToManifest(int manifestFD, Manifest* manifestList)
{
	//Write versionNumber of .Manifest
	char versionNumber[10];
	memset(versionNumber, '\0', 10 * sizeof(char));
	sprintf(versionNumber, "%d", manifestList->versionNum);
	write(manifestFD, versionNumber, strlen(versionNumber));
	write(manifestFD, "\n", 1);

	//Write numFiles in .Manifest
	char numFiles[10];
	memset(numFiles, '\0', 10 * sizeof(char));
	sprintf(numFiles, "%d", manifestList->numFiles);
	write(manifestFD, numFiles, strlen(numFiles));
	write(manifestFD, "\n", 1);

	//Write file entries: <filePath>:<verNum>:<hash>:<code>
	ManifestEntryNode* curr = manifestList->head;
	while(curr != NULL)
	{
		write(manifestFD, curr->filePath, strlen(curr->filePath));
		write(manifestFD, ":", 1);

		write(manifestFD, curr->versionNum, strlen(curr->versionNum));
		write(manifestFD, ":", 1);

		write(manifestFD, curr->fileHash, strlen(curr->fileHash));
		write(manifestFD, ":", 1);

		write(manifestFD, curr->manifestCode, strlen(curr->manifestCode));
		write(manifestFD, "\n", 1);

		curr = curr->next;
	}

}

static int compareManifests(Manifest* serverMan, Manifest* clientMan, char* pathToProj, int updateFD) {

	char* pathToConflict = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + strlen("/.Conflict"))));
	sprintf(pathToConflict, "%s/%s", pathToProj, ".Conflict");
	int conflictFD = open(pathToConflict, O_CREAT | O_WRONLY, 00777);
	int conflictCount = 0;

	ManifestEntryNode* clientEntryNode;
	ManifestEntryNode *curr = serverMan->head;
	while(curr != NULL) {
		// Search client file in Server
		clientEntryNode = findNodeByFilePath(clientMan, curr->filePath);
		char* liveHash;

		if(clientEntryNode == NULL)
		{
			write(updateFD, "A:", 2);

			write(updateFD, curr->filePath, strlen(curr->filePath));
			write(updateFD, ":", 1);

			write(updateFD, curr->versionNum, strlen(curr->versionNum));
			write(updateFD, ":", 1);

			write(updateFD, curr->fileHash, strlen(curr->fileHash));
			
			printf("A %s\n", curr->filePath);
		}
		else if(strcmp(curr->manifestCode, "N") == 0)
		{
			//skip over this one
			curr = curr->next;
			continue;
		}
		else if(strcmp(curr->manifestCode, "R") == 0)
		{
			write(updateFD, "D:", 2);

			write(updateFD, curr->filePath, strlen(curr->filePath));
			write(updateFD, ":", 1);

			write(updateFD, curr->versionNum, strlen(curr->versionNum));
			write(updateFD, ":", 1);

			write(updateFD, curr->fileHash, strlen(curr->fileHash));
			
			printf("D %s\n", curr->filePath);
		}
		else if(strcmp(curr->manifestCode, "M") == 0)
		{
			//get live hash
			char* pathToFile = clientEntryNode->filePath;
			liveHash = strdup(hashFile(pathToFile));

			//check if live hash is the same as client hash, if it is, increment curr and continue (treat as N)
			if(strcmp(liveHash, clientEntryNode->fileHash) != 0)
			{
				printf("Live Hash: %s\n", liveHash);
				printf("Hash in Client .Manifest: %s\n", clientEntryNode->fileHash);
				//write this to .Conflict
				write(conflictFD, "C:", 2);

				write(conflictFD, curr->filePath, strlen(curr->filePath));
				write(conflictFD, ":", 1);

				write(conflictFD, curr->versionNum, strlen(curr->versionNum));
				write(conflictFD, ":", 1);

				write(conflictFD, curr->fileHash, strlen(curr->fileHash));
				
				printf("C %s\n", curr->filePath);
				conflictCount ++;
			}
			//if they do match...
			else
			{
				write(updateFD, "M:", 2);

				write(updateFD, curr->filePath, strlen(curr->filePath));
				write(updateFD, ":", 1);

				write(updateFD, curr->versionNum, strlen(curr->versionNum));
				write(updateFD, ":", 1);

				write(updateFD, curr->fileHash, strlen(curr->fileHash));
			
				printf("M %s\n", curr->filePath);
			}

		}

		curr = curr->next;
	}
	if(conflictCount == 0)
	{
		close(conflictFD);
		remove(pathToConflict);
	}
	else
	{
		printf("Error: Conflicts were found and must be resolved before project can be updated.\n");
	}
	close(conflictFD);
	close(updateFD);
	
}


static void freeManifestEntryNodes(Manifest* manifestList)
{
	ManifestEntryNode* tmp;
	while(manifestList->head != NULL)
	{
		tmp = manifestList->head;
		manifestList->head = manifestList->head->next;
		if(tmp->filePath != NULL)
		{
			free(tmp->filePath);
		}
		if(tmp->fileHash != NULL)
		{
			free(tmp->fileHash);
		}
		if(tmp->versionNum != NULL)
		{
			free(tmp->versionNum);
		}
		if(tmp->manifestCode != NULL)
		{
			free(tmp->manifestCode);
		}
		free(tmp);
	}
}

#endif
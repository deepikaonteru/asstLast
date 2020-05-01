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
	char *projName;
	char *versionNum;
	int numFiles;
	ManifestEntryNode *head;
	ManifestEntryNode *tail;
} Manifest;

void addEntryToManifest(Manifest *manifestList, char *filePath, char *versionNum, char *fileHash, char *manifestCode) {
	
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

//Ex:
    //1
	//<numFiles>
    //<filePath>:<verNum>:<hash>:<code>

Manifest *readManifest(int fd) {
	
	SocketBuffer *socketBuffer = createBuffer();
	
	Manifest *manifestList = malloc(sizeof(Manifest));
	manifestList->head = NULL;
	manifestList->tail = NULL;
	manifestList->numFiles = 0;

	// first line is versionNumber
	readTillDelimiter(socketBuffer, fd, '\n');
	manifestList->versionNum = readAllBuffer(socketBuffer);

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
		
		readTillDelimiter(socketBuffer, fd, ':');
		versionNum = readAllBuffer(socketBuffer);

		readTillDelimiter(socketBuffer, fd, ':');
		fileHash = readAllBuffer(socketBuffer);
		
		readTillDelimiter(socketBuffer, fd, '\n');
		manifestCode = readAllBuffer(socketBuffer);

		addEntryToManifest(manifestList, filePath, versionNum, fileHash, manifestCode);
	}
	
	// free buffer
	freeSocketBuffer(socketBuffer);
	return manifestList;

}
	
typedef struct SocketNode {
	char c;
	struct SocketNode *next;
} SocketNode;

typedef struct SocketBuffer {
	int size;
	SocketNode *head;
	SocketNode *tail;
} SocketBuffer;

static ManifestEntryNode* insertAtHead(char* entryBuffer, int numEntries)
{
	ManifestEntryNode* node;

	return node;
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

#endif
#include <unistd.h> 
#include <stdio.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h> 
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "socketBuffer.h"
#include "zlibCompDecomp.h"
char SERVER_REPOS[] = "./serverSide/serverRepos";
char DOT_VERSION[] = ".version";
char DOT_HISTORY[] = ".history";
char DOT_COMMITS[] = ".Commits";
char DOT_PROJECT[] = ".project";
long int id = 0;

char *readFileContents(char *fileName) {
	if(!fileExistsCheck(fileName)) {
		printf("Unable to read file contents. File %s does not exist.", fileName);
		return NULL;
	}
	char *result = malloc(sizeof(char) * (findFileSize(fileName) + 10));
	
	// Now write file char by char on the socket		
	int fileFd = open(fileName, O_RDONLY, 00777);
	int i = 0;
	while (read(fileFd, &result[i], 1) == 1) {
		i++;
	}

	close(fileFd);
	result[i] = '\0';
    //printf("%s\n", result);
	return result;

}

char *readCurrentVersion(char *projName) {
	char *path = malloc(sizeof(char) * (strlen(projName) + strlen(DOT_VERSION) + 40 + strlen(SERVER_REPOS)));
	sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, DOT_VERSION);
	
	char *result = readFileContents(path);
	free(path);
	return result;
}

int DirExistsCheck(char* dirName)
{
    struct stat stats;
    int res = 0;

    // Check for dir existence
    if (stat(dirName, &stats) == 0 && S_ISDIR(stats.st_mode))
        res = 1;

    //printf("%d\n", res);
    return res;
}

int fileExistsCheck(char *fileName) {
	struct stat buffer;
	return (stat(fileName, &buffer) == 0);
}

void writeFileToSocket(int sock, char *filePath) {
    //Find fileName
    char* fileName = strrchr(filePath, '/') + 1; //./serverRepos/test/1/.Manifest
    //Find lenOfFileName
    int lenFileName = strlen(fileName);
    //Find numBytesInFile
    long fileSize = findFileSize(filePath);

    //Write this stuff to socket
    char* fileInfo = (char*)(malloc(sizeof(char) * (strlen(fileName) + 5 + 25 + 3)));
    sprintf(fileInfo, "%d:%s:%ld:", lenFileName, fileName, fileSize);
    write(sock, fileInfo, strlen(fileInfo));

    free(fileInfo);

    //Get the contents of the file
    int fd = open(filePath, O_RDONLY, 00777);
    char* readIn = (char*)(malloc(sizeof(char) * fileSize));
    int numBytesRead = read(fd, readIn, fileSize);
    close(fd);

    //Write contents 
    write(sock, readIn, numBytesRead);
    free(readIn);
}


void writeFToSocket(int sock, char *projName, const char *fileExtension) {
	
	// Check first if project exists.
	printf("Writing File %s in project: %s to client.\n", fileExtension, projName);
	char *path = malloc(sizeof(char) * (strlen(projName) + strlen(fileExtension) + 50 + strlen(SERVER_REPOS)));
	
	// Read current version of project.
	char *version = readCurrentVersion(projName);

	// Create file path on server.
	sprintf(path, "%s/%s/%s/%s", SERVER_REPOS, projName, version, fileExtension);
    //printf("%s\n", path);
	free(version);
	
	long fileSize = findFileSize(path);	
    
    //Get the contents of the file		
	int fd = open(path, O_RDONLY, 00777);
    char* readIn = (char*)(malloc(sizeof(char) * fileSize));
    int numBytesRead = read(fd, readIn, fileSize);
    //printf("%s\n", readIn);
    close(fd);

    char* fileInfo = (char*)(malloc(sizeof(char) * (16 + strlen(fileExtension) + 16)));
	sprintf(fileInfo, "%d:%s:%ld:", strlen(fileExtension), fileExtension, fileSize);
	write(sock, fileInfo, strlen(fileInfo));
    //printf("%s\n", readIn);
		
    //Write contents 
    write(sock, readIn, numBytesRead);
    //sendfile:1:9:.Manifest:<numBytesRead>:<readIn>

    //fin:1:9:.Manifest:<numBytesRead>:<readIn>

	free(path);
}


// failed: error:
void writeErrorToSocket(int sock, char *error) {
	write(sock, "failed:", strlen("failed:"));
	write(sock, error, strlen(error));
	write(sock, ":", 1);
}

int removeDir(char *path) {

   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;

      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          struct stat statbuf;

          snprintf(buf, len, "%s/%s", path, p->d_name);

          if (!stat(buf, &statbuf)) {
             if (S_ISDIR(statbuf.st_mode)) {
                r2 = removeDir(buf);
             } else {
                r2 = unlink(buf);
             }
          } else {
			  printf("Stat error while deleting complete folder: %s\n", buf);
			  fflush(stdout);
		  }

          free(buf);
          r = r2;
      }

   } else {
	  printf("Could not open dir while deleting complete folder: %s.\n", path);
	  fflush(stdout);
   }
   closedir(d);

   if (!r) {
      r = rmdir(path);
   }

   return r;
}


void serverDestroy(char* projName, int sock){

    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));

    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
    //printf("%s\n",path);

    if(!ProjInServerRepos(projName)) 
    {
		writeErrorToSocket(sock, "Project does not exist.");
        free(path);
	} 
    else 
    {
		char *path = malloc(sizeof(char) * (strlen(projName) + 50 + strlen(SERVER_REPOS)));
		sprintf(path, "%s/%s", SERVER_REPOS, projName);
			
        //removeDir(path);
        char* sysCmd = (char*)(malloc(sizeof(char) * (strlen("rm -rf ") + strlen(path))));
        sprintf(sysCmd, "rm -rf %s", path);
        system(sysCmd);
            
        free(path);
			
		write(sock, "destroyed:", strlen("destroyed:")); 
	}
		
}

int ProjInServerRepos(char *projName) {

	char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
	sprintf(path, "%s/%s", SERVER_REPOS, projName);
	int result = DirExistsCheck(path);
	free(path);
	return result;

}

void serverUpgrade(char* projName, int sock)
{
     //if it does not exist on server, server reports that project DNE, end command
    char* pathToProj = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(pathToProj, SERVER_REPOS);
    strcat(pathToProj, "/");
    strcat(pathToProj, projName);

	if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");
	}
    else {
        //server expects- sendPath:<filePathLength>:<filePath>
        //OR: fin:
        char* cv = readCurrentVersion(projName);
        //printf("%s\n", cv);
        write(sock, cv, strlen(cv));
        write(sock, ":", 1);

        SocketBuffer* socketBuffer = createBuffer();
        int loopEnabled = 1;
        while(loopEnabled)
        {
            readTillDelimiter(socketBuffer, sock, ':');
            char* response = readAllBuffer(socketBuffer);
            if(strcmp(response, "fin") == 0) {
                loopEnabled = 0;
                clearSocketBuffer(socketBuffer);
                printf("Server successfully sent updated project to client side.\n");
                break;
            }
            else {
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
                //printf("%s\n", fileName);
                char* pathOfFileToSend = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(cv) + 1 + strlen(fileName))));
                sprintf(pathOfFileToSend, "%s/%s/%s/%s", SERVER_REPOS, projName, cv, fileName);
                long int fileSize = findFileSize(pathOfFileToSend);
                char fileSizeBuf[11];
                memset(fileSizeBuf, '\0', sizeof(char) * 11);
                sprintf(fileSizeBuf, "%ld", fileSize);
                
                int fileFD = open(pathOfFileToSend, O_RDONLY, 00777);
                char* fileContent = (char*)(malloc(sizeof(char) * fileSize));
                read(fileFD, fileContent, fileSize);
                //printf("%s\n", fileContent);
                close(fileFD);
                
                char* msg = (char*)(malloc(sizeof(char) * (strlen("sendfile:") + strlen(fileName) + 1 + strlen(fileSizeBuf) + 1 + strlen(fileContent))));
                sprintf(msg, "sendfile:%s:%s:%s", fileName, fileSizeBuf, fileContent);
                //printf("%s\n", msg);

                send(sock, msg, strlen(msg), 0);
            }

        }
        
    }
}

void serverCheckout(char* projName, int sock)
{
    //if it does not exist on server, server reports that project DNE, end command
    char* pathToProj = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(pathToProj, SERVER_REPOS);
    strcat(pathToProj, "/");
    strcat(pathToProj, projName);

	if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");

	}
    else {

         // get current version from readCurrentVersion()
        char* cv = readCurrentVersion(projName);

        // make a path to that current version folder for that project
        char* pathToCVManifest = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(cv) + strlen("/.Manifest"))));
        sprintf(pathToCVManifest, "%s/%s/.Manifest", pathToProj, cv);

        long int manifestFileSize = findFileSize(pathToCVManifest);
        int manifestFD = open(pathToCVManifest, O_RDONLY, 00777);
        Manifest* projManifestList = readManifest(manifestFD);
        close(manifestFD);

        write(sock, "sendProj:", strlen("sendProj:"));
        writeFToSocket(sock, projName, ".Manifest");

        //Using manifest struct, loop through each node, send each curr->filePath content to client
        //so that it can make a copy
        ManifestEntryNode* curr = projManifestList->head;
        while(curr != NULL)
        {
            //retrieve filePath
            char* filePath = curr->filePath;

            //retrieve fileName, fileSize, fileContent
            char* fileName = strrchr(filePath, '/') + 1;
            char* serverFilePath = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(cv) + 1 + strlen(fileName))));
            sprintf(serverFilePath, "%s/%s/%s/%s", SERVER_REPOS, projName, cv, fileName);
            //printf("%s\n", fileName);
            char fileSize[11];
            long fSize = findFileSize(serverFilePath);
            memset(fileSize, '\0', sizeof(char) * 11);
            sprintf(fileSize, "%ld", fSize);
            //printf("%s\n", fileSize);
            //printf("yer\n");
            //printf("%s\n", serverFilePath);
            char* fileContent = (char*)(malloc(sizeof(char) * fSize));
            int fileFD = open(serverFilePath, O_RDONLY, 00777);
            read(fileFD, fileContent, fSize);
            close(fileFD);
            //printf("yer\n");

            //send this data in a message
            write(sock, "sendfile:", strlen("sendfile:"));
            write(sock, fileName, strlen(fileName));
            write(sock, ":", 1);
            write(sock, fileSize, strlen(fileSize));
            write(sock, ":", 1);
            write(sock, fileContent, strlen(fileContent));

            curr = curr->next;

        }

        send(sock, "fin:", strlen("fin:"), 0);
        printf("Files have been sent successfully.\n");

/*
        write(sockfd, "sendProject", strlen("sendProject:"));

        //we want to compress everything now

        char* pathToDotProject = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(DOT_PROJECT))));
        sprintf(pathToDotProject, "%s/%s", pathToProj, DOT_PROJECT);
        //printf("%s\n", pathToDotProject);

        int projectFD = open(pathToDotProject, O_CREAT | O_WRONLY | O_TRUNC, 0777);

        // Add 1 to include manifest file
        char[11] = nfBuff;
		sprintf(nfBuff, "%d:", 1 + projManifestList->numFiles);
		write(projectFD, nfBuff, strlen(nfBuff));

        writeFToSocket(projectFD, projName, DOT_MANIFEST);

        ManifestEntryNode* curr = projManifestList->head;

        while(curr!=NULL) {
            writeFileToSocket(projectFD, projName, curr->filePath);
			curr = curr->next;
        }

        close(projectFD);

        compressProject(sock, pathToDotProject, pathToProj);

        unlink(pathToDotProject);
        free(pathToDotProject);
*/




        /*
        char* cv = readCurrentVersion(projName);
        char* pathToProjCV = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(cv))));
        sprintf(pathToProjCV, "%s/%s", pathToProj, cv);
        //printf("%s\n", pathToProjCV);

        char* sysCMD = (char*)(malloc(sizeof(char) * (strlen("tar -zcvf ") + strlen(projName) + strlen(".tar.gz ") + strlen(pathToProjCV))));
        sprintf(sysCMD, "tar -zcvf %s.tar.gz %s", projName, pathToProjCV);
        //printf("%s\n", sysCMD);
        system(sysCMD);

        char* pathToTar = (char*)(malloc(sizeof(char) * (2 + strlen(projName) + strlen(".tar.gz"))));
        sprintf(pathToTar, "./%s.tar.gz", projName);

        long int sizeTarFile = findFileSize(pathToTar);
        char sTarFile[11];
        memset(sTarFile, '\0', 11 * sizeof(char));
        sprintf(sTarFile, "%ld:", sizeTarFile);
        SocketBuffer* socketBuffer = createBuffer();
        int tarFD = open(pathToTar, O_RDONLY, 00777);
        readNBytes(socketBuffer, tarFD, sizeTarFile);
        char* tarBuffer = readAllBuffer(socketBuffer);
        //printf("%s\n", tarBuffer);
        //printf("%d\n", read(tarFD, tarContents, sizeTarFile));
        //printf("%d\n", strlen(tarContents));
        close(tarFD);
        char* sendSize = (char*)(malloc(sizeof(char) * strlen(sTarFile) + 1));
        sprintf(sendSize, "%s:", sTarFile);
        write(sock, sendSize, strlen(sendSize));

        readTillDelimiter(socketBuffer, sock, ':');
        char* clientResponse = readAllBuffer(socketBuffer);

        send(sock, tarBuffer, sizeTarFile, 0);

        */

        //send(sock, tarBuffer, sizeTarFile, 0);
          //if it DOES exist...
            //need a way to compress the entire project folder for the latest version
            //could use system(tar...) to compress whole folder on serverSide, then...
            //send that compressed file over to clientSide and decompress it
            //extract all files, rename the folder from <versionNumber> to <projectName>

            /*
            // get current version from readCurrentVersion()
            char* cv = readCurrentVersion(projName);

            // make a path to that current version folder for that project
            char* pathToCVManifest = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(cv) + strlen("/.Manifest"))));
            sprintf(pathToCVManifest, "%s/%s/.Manifest", pathToProj, cv);

            //char* pathToCV = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(cv))));
            //sprintf(pathToCV, "%s/%s", pathToProj, cv);
            //printf("%s\n", pathToCV);

            char* pathToDotProject = (char*)(malloc(sizeof(char) * (strlen(pathToProj) + 1 + strlen(DOT_PROJECT))));
            sprintf(pathToDotProject, "%s/%s", pathToProj, DOT_PROJECT);
            //printf("%s\n", pathToDotProject);

            long int manifestFileSize = findFileSize(pathToCVManifest);
            int manifestFD = open(pathToCVManifest, O_RDONLY, 00777);
            Manifest* projManifestList = readManifest(manifestFD);
            close(manifestFD);

            int dotProjectFD = open(pathToDotProject, O_CREAT | O_WRONLY, 00777);
            ManifestEntryNode* curr = projManifestList->head;
            while(curr != NULL)
            {
                char* currFile = curr->filePath;
                char* currFileName = strrchr(currFile, '/') + 1;
                long int currFileSize = findFileSize(currFile);
                char cfSize[11];
                memset(cfSize, '\0', 11 * sizeof(char));
                sprintf(cfSize, "%ld", currFileSize);
                int fileFD = open(currFile, O_RDONLY, 00777);

                char* currFileContent = (char*)(malloc(sizeof(char) * currFileSize));
                read(fileFD, currFileContent, currFileSize);
                close(fileFD);

                write(dotProjectFD, currFileName, strlen(currFileName));
                write(dotProjectFD, ":", 1);

                write(dotProjectFD, cfSize, strlen(cfSize));
                write(dotProjectFD, ":", 1);

                write(dotProjectFD, currFileContent, strlen(currFileContent));
                write(dotProjectFD, "\n", 1);

                curr = curr->next;
            }
            close(dotProjectFD);

            write(sock, "sendProject:", strlen("sendProject:"));
            write(sock, "2:", 2);
            writeFToSocket(sock, projName, ".Manifest");
            write(sock, ":", 1);
            compressProject(sock, pathToDotProject, pathToProj);

            /*
            // get its manifest contents and all its contents compressed
            int manifestFD = open(pathToCVManifest, O_RDONLY, 00777);
            Manifest* projManifest = readManifest(manifestFD);
            int numFilesSending = projManifest->numFiles + 1;
            char nF[11];
            sprintf(nF,"%d:", numFilesSending);
            //printf("%s\n",nF);
            write(sock, nF, strlen(nF));

            //long int manifestSize = findFileSize(pathToCVManifest);
            //char mSize[11];
            //memset(mSize, '\0', 11 * sizeof(char));
            //sprintf(manifestSize, "%ld:", manifestSize);
            //write(sock, mSize, strlen(mSize));
            write(sock, ".Manifest:", strlen(".Manifest:"));
            compressProject(sock, pathToCVManifest, pathToProj);
            write(sock, ":", 1);
            close(manifestFD);

            // loop through manifest struct
            ManifestEntryNode* currEntry = projManifest->head;
            while(currEntry != NULL)
            {
                char* fileToCompress = currEntry->filePath;
                printf("%s\n", fileToCompress);
                char* fileName = strrchr(fileToCompress, '/') + 1;
                write(sock, fileName, strlen(fileName));
                write(sock, ":", 1);
                compressProject(sock, fileToCompress, pathToProj);
                printf("%s\n", fileName);
                currEntry = currEntry->next;
            }

			//write(sock, "sendProject:", strlen("sendProject:"));*/



	}


}

void serverUpdate(char* projName, int sock)
{
    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
		
	if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");
			
	} else {
		// send manifest file back
        id ++;
		write(sock, "sendfile:", strlen("sendfile:"));
		write(sock, "1:", 2);
		writeFToSocket(sock, projName, ".Manifest");
	}
}
void serverCommit(char* projName, int sock)
{
    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
		
	if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");
			
	} else {
		// send manifest file back
        id ++;
		write(sock, "sendfile:", strlen("sendfile:"));
		write(sock, "1:", 2);
		writeFToSocket(sock, projName, ".Manifest");
        char ID[11];
        memset(ID, '\0', 11);
        sprintf(ID, "%ld:", id);
        write(sock, ID, strlen(ID));
	}
    
    //Wait for .Commit file to be sent, OR failure message
    // sendfile:<numFiles>:<ManifestNameLen>:<manifest name>:<numBytesOfContent>:<contents>
	// OR "failed:<fail Reason>:"
 
    SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sock, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		//<numFiles>:<lengthOfCommitName>:.Commit:<sizeOfCommit>:<contentOfCommit>
        //The client should output a list of all
        //files under the project name, along with their version number (i.e., number of updates).
        readTillDelimiter(socketBuffer, sock, ':');
        char* numFiles = readAllBuffer(socketBuffer);
        int nF = atoi(numFiles);

        readTillDelimiter(socketBuffer, sock, ':');
        char* lenExt = readAllBuffer(socketBuffer);
        long lExt = atol(lenExt);

        readTillDelimiter(socketBuffer, sock, ':');
        char* ext = readAllBuffer(socketBuffer);
        
        readTillDelimiter(socketBuffer, sock, ':');
        char* numBytesContent = readAllBuffer(socketBuffer);
        long nBytesContent = atol(numBytesContent);

        // Read .Commit's contents into a something and write to .Commit
        readNBytes(socketBuffer, sock, nBytesContent);
        char* content = readAllBuffer(socketBuffer);
        
        //Write these contents into a .Commit file within the server repo
        //build path to .Commits folder in project
        char* pathToCommit = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_COMMITS) + 1 + strlen(ext))));
        sprintf(pathToCommit, "%s/%s/%s/%s", SERVER_REPOS, projName, DOT_COMMITS, ext);
        //printf("%s\n", pathToCommitDir);

        //Open file specified by pathToCommitDir and write contents in content buffer to that file
        int commitFD = open(pathToCommit, O_CREAT | O_WRONLY, 00777);
        write(commitFD, content, strlen(content));
        close(commitFD);
        printf("Commit saved successfully.\n");
        free(pathToCommit);
	} else {
		printf("Could not get .Commit from server.\n");		
		readTillDelimiter(socketBuffer, sock, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}

void serverPush(char* projName, int sock)
{
    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);

    if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");		
	}
    else {
        SocketBuffer* socketBuffer = createBuffer();
        readTillDelimiter(socketBuffer, sock, ':');
        char* ID = readAllBuffer(socketBuffer);
        printf("Searching project for .Commit%s\n", ID);

        char* pathToCommit = (char*)(malloc(sizeof(char) * (strlen(path) + 1 + strlen(DOT_COMMITS) + 1 + strlen(".Commit") + strlen(ID))));
        sprintf(pathToCommit, "%s/%s/%s%s", path, DOT_COMMITS, ".Commit", ID);
        int commitFD = open(pathToCommit, O_RDONLY, 00777);
        if(commitFD == -1)
        {
            writeErrorToSocket(sock, ".Commit file does not exist in server.");
            close(commitFD);
            return;
        }
        close(commitFD);
        //printf("Server found the .Commit file\n");
        send(sock, "Server found the .Commit file\n", strlen("Server found the .Commit file\n"), 0);

        //make new directory for new version of project
        char* pathToDotVersion = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_VERSION))));
        sprintf(pathToDotVersion, "%s/%s/%s", SERVER_REPOS, projName, DOT_VERSION);
        char verNumber[11];
        memset(verNumber, '\0', 11 * sizeof(char));
        int verFD = open(pathToDotVersion, O_RDONLY, 00777);
        read(verFD, verNumber, findFileSize(pathToDotVersion));
        close(verFD);

        //make path to old version, obtain old .Manifest
        char* pathToOldManifest = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(verNumber) + 1 + strlen(".Manifest"))));
        sprintf(pathToOldManifest, "%s/%s/%s/.Manifest", SERVER_REPOS, projName, verNumber);
        int manifestFD = open(pathToOldManifest, O_RDONLY, 00777);
        Manifest* manifest = readManifest(manifestFD);
        close(manifestFD);

        int newVerNumber = atoi(verNumber) + 1;
        sprintf(verNumber, "%d", newVerNumber);
        verFD = open(pathToDotVersion, O_CREAT | O_WRONLY, 00777);
        write(verFD, verNumber, strlen(verNumber));
        close(verFD);
        char* pathToNewVerDir = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(verNumber))));
        sprintf(pathToNewVerDir, "%s/%s/%s", SERVER_REPOS, projName, verNumber);
        int newVerDirectory = mkdir(pathToNewVerDir, 00777);

        clearSocketBuffer(socketBuffer);

        commitFD = open(pathToCommit, O_RDONLY, 00777);
        readTillDelimiter(socketBuffer, commitFD, '\n');
        char* numIDinCommit = readAllBuffer(socketBuffer);
        clearSocketBuffer(socketBuffer);
        char* manifestCode = (char*)(malloc(sizeof(char) * 3));
        memset(manifestCode, '\0', 3 * sizeof(char));
        while(read(commitFD, manifestCode, 2) == 2)
        {
            //printf("%s\n", manifestCode);
            //if manifestCode is D, ignore this entry of the .Commit, do not ask server for this
            if(strcmp(manifestCode, "D:") == 0)
            {
                //filePath
                readTillDelimiter(socketBuffer, commitFD, ':');
                char* fp = readAllBuffer(socketBuffer);
                clearSocketBuffer(socketBuffer);
                
                //fileVersion
                readTillDelimiter(socketBuffer, commitFD, ':');
                clearSocketBuffer(socketBuffer);

                //fileHash
                readTillDelimiter(socketBuffer, commitFD, '\n');
                clearSocketBuffer(socketBuffer);

                removeEntryFromManifest(manifest, fp);

                continue;
            }

            //read filePath
            readTillDelimiter(socketBuffer, commitFD, ':');
            char* filePath = readAllBuffer(socketBuffer);
            //printf("%s\n", filePath);
            clearSocketBuffer(socketBuffer);

            //char* filePathMSG = (char*)(malloc(sizeof(char) * (strlen("sendPath:") + strlen(filePath))));
            //sprintf(filePathMSG, "sendPath:%s", filePath);
            //printf("%s\n", filePathMSG);

            //send this filePath to the server so that it can send its contents back to here
            //send(sock, filePath, strlen(filePath), 0);
            char filePathLen[11];
            memset(filePathLen, '\0', 11 * sizeof(char));
            sprintf(filePathLen, "%d:", strlen(filePath));
            write(sock, "sendPath:", strlen("sendPath:"));
            write(sock, filePathLen, strlen(filePathLen));
            write(sock, filePath, strlen(filePath));
            //free(filePathMSG);

            //read in versionNum
            readTillDelimiter(socketBuffer, commitFD, ':');
            char* verNum = readAllBuffer(socketBuffer);
            clearSocketBuffer(socketBuffer);

            //read in hashCode
            readTillDelimiter(socketBuffer, commitFD, '\n');
            char* fHash = readAllBuffer(socketBuffer);
            clearSocketBuffer(socketBuffer);

            //if checks for adding manifest node
            if(strcmp(manifestCode, "A:") == 0)
            {
                //adding file, need to create new entry
                addEntryToManifest(manifest, filePath, verNum, fHash, "N");
            }
            else if(strcmp(manifestCode, "M:") == 0)
            {
                ManifestEntryNode* modifiedEntry = findNodeByFilePath(manifest, filePath);
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
            readTillDelimiter(socketBuffer, sock, ':');
            clearSocketBuffer(socketBuffer);

            //fileName
            readTillDelimiter(socketBuffer, sock, ':');
            char* fileName = readAllBuffer(socketBuffer);
            clearSocketBuffer(socketBuffer);

            //fileSize
            readTillDelimiter(socketBuffer, sock, ':');
            char* fileSize = readAllBuffer(socketBuffer);
            long fSize = atol(fileSize);
            clearSocketBuffer(socketBuffer);

            //fileContent
            char* fileContent = (char*)(malloc(sizeof(char) * fSize));
            read(sock, fileContent, fSize);
            //printf("%s\n", fileContent);

            //start to write contents to file
            char* pathToFileInNewVerDir = (char*)(malloc(sizeof(char) * (strlen(pathToNewVerDir) + 1 + strlen(fileName))));
            sprintf(pathToFileInNewVerDir, "%s/%s", pathToNewVerDir, fileName);
            int fileFD = open(pathToFileInNewVerDir, O_CREAT | O_WRONLY, 00777);
            write(fileFD, fileContent, strlen(fileContent));

        }

        close(commitFD);

        //write new .Manifest
        manifest->versionNum ++;
        char* pathToNewManifest = (char*)(malloc(sizeof(char) * (strlen(pathToNewVerDir) + 1 + strlen(".Manifest"))));
        sprintf(pathToNewManifest, "%s/.Manifest", pathToNewVerDir);
        manifestFD = open(pathToNewManifest, O_CREAT | O_WRONLY, 00777);
        writeToManifest(manifestFD, manifest);
        close(manifestFD);

        //need to send msg to client stating that we finished reading the .Commit
        printf("Finished reading .Commit file\n");
        send(sock, "fin:1:", strlen("fin:1:"), 0);

        //write .Manifest to the socket
        writeFToSocket(sock, projName, ".Manifest");
        
        //expire commits
        char* pathToRemove = (char*)(malloc(sizeof(char) * (strlen(SERVER_REPOS) + 1 + strlen(projName) + 1 + strlen(DOT_COMMITS))));
        sprintf(pathToRemove, "%s/%s/%s", SERVER_REPOS, projName, DOT_COMMITS);

        char* sysCmd = (char*)(malloc(sizeof(char) * (strlen("rm -rf ") + strlen(pathToRemove))));
        sprintf(sysCmd, "rm -rf %s", pathToRemove);
        system(sysCmd);

        mkdir(pathToRemove, 00777);

        printf("Project successfully updated on server side.\n");

        send(sock, "done", strlen("done"), 0);

        /*
        id ++;
        write(sock, "sendfile:", strlen("sendfile:"));
        write(sock, "1:", 2);
        writeFToSocket(sock, projName, ".Manifest");
        char ID[11];
        memset(ID, '\0', 11);
        sprintf(ID, "%ld:", id);
        write(sock, ID, strlen(ID));*/
	}
}

void serverCurrentVersion(char* projName, int sock)
{
    char* path = (char*)(malloc((strlen(SERVER_REPOS) + strlen("/") + strlen(projName) + 75) * sizeof(char)));
    strcpy(path, SERVER_REPOS);
    strcat(path, "/");
    strcat(path, projName);
		
	if(!ProjInServerRepos(projName)) {
		writeErrorToSocket(sock, "Project does not exist.");
			
	} else {
		// send manifest file back
		write(sock, "sendfile:", strlen("sendfile:"));
		write(sock, "1:", 2);
		writeFToSocket(sock, projName, ".Manifest");
	}
		
	free(path);

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
            free(path);
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
        int vfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 00777);	
        write(vfd, version, strlen(version));
        close(vfd);

        // make history file 
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, DOT_HISTORY);
        int hfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 00777);
        close(hfd);
        
        //create directory for commits
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, ".Commits");
        mkdir(path, 00777);
	
        // create version directory
        sprintf(path, "%s/%s/%s", SERVER_REPOS, projName, version);
        mkdir(path, 00777);

        /*char* pathToManifest = (char*)(malloc((strlen(path) + strlen("/.Manifest")) * sizeof(char)));
        strcpy(pathToManifest, path);
        strcat(pathToManifest, "/.Manifest");*/

        // place .Manifest inside version directory
        sprintf(path, "%s/%s/%s/%s", SERVER_REPOS, projName, version, ".Manifest");

        int manifestFD = open(path, O_WRONLY | O_CREAT, 00600);
        write(manifestFD, "1\n0\n", 4);
        close(manifestFD);
        write(sock, "sendfile:", strlen("sendfile:"));
	    write(sock, "1:", 2); 
		writeFileToSocket(sock, path);
        free(path);
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
                //printf("%s\n", cmdBuf);

                //Step 4: Determine what command is in cmdBuf. Based on that command, do said action.
                if(strcmp(cmdBuf, "cr") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverCreate(projName, newSocket);
                }

                if(strcmp(cmdBuf, "des") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverDestroy(projName, newSocket);
                }

                if(strcmp(cmdBuf, "cv") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverCurrentVersion(projName, newSocket);
                }

                if(strcmp(cmdBuf, "com") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverCommit(projName, newSocket);
                }

                if(strcmp(cmdBuf, "co") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverCheckout(projName, newSocket);
                }

                if(strcmp(cmdBuf, "upd") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverUpdate(projName, newSocket);
                }

                 if(strcmp(cmdBuf, "upg") == 0)
                {
                    char* projName = strchr(fullCmdBuf, ':') + 1;
                    serverUpgrade(projName, newSocket);
                }

                if(strcmp(cmdBuf, "push") == 0)
                {
                    char* projName = strrchr(fullCmdBuf, ':') + 1;
                    serverPush(projName, newSocket);
                }

                if(strcmp(cmdBuf, "roll") == 0)
                {
                    //<lengthAfterFirstColon>:roll:<projName>:<version>
                    //char* projName = strrchr(fullCmdBuf, ':') + 1;
                    //char* version = 
                    //serverRollback(projName, version, snewSocket);
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
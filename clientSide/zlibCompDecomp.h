#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

static void writeNBytesToFile(long nBytes, int sockToRead, int sockToWrite) {
    /*
	while(nBytes-- > 0) {
		char c;
		read(sockToRead, &c, 1);
		write(sockToWrite, &c, 1);
	}*/
    char* readIn = (char*)(malloc(sizeof(char) * nBytes));
    read(sockToRead, readIn, nBytes);
    write(sockToWrite, readIn, nBytes);
}

long int findFileSize(char *fileName) {
		
	struct stat buffer;
	int status = stat(fileName, &buffer);
	// if permission available
	if(status == 0) {
		return buffer.st_size;
	}
	return -1;
}

// Compress the input file and write data on output file
static void compressFile(char *source, char *dest)
{
	int readFd = open(source, O_RDONLY, 00777);
	int writeFd = open(dest, O_CREAT | O_WRONLY | O_TRUNC, 00777);	
	
    int ret, flush;
    unsigned numBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
		printf("Error in ZLIB\n");
		close(readFd);
		close(writeFd);
        return;
	}

    /* compress until end of file */
    do {
        strm.avail_in = read(readFd, in, CHUNK);
        if (strm.avail_in < 0) {
            (void)deflateEnd(&strm);
			printf("Error in Reading file %s\n", source);
			close(readFd);
			close(writeFd);
            return;
        }
        flush = (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            numBytes = CHUNK - strm.avail_out;   // numBytes to write on output.
			write(writeFd, out, numBytes);
			
        } while (strm.avail_out == 0);

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
	
	close(readFd);
	close(writeFd);
}


// DeCompress the input file and write data on output file
static void decompressFile(char *source, char *dest)
{
	int readFd = open(source, O_RDONLY, 00777);
	int writeFd = open(dest, O_CREAT | O_WRONLY | O_TRUNC, 00777);	
	
    int ret;
    unsigned numBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
		printf("Error in ZLIB\n");
		close(readFd);
		close(writeFd);
        return;
	}

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = read(readFd, in, CHUNK);
        if (strm.avail_in < 0) {
            (void)deflateEnd(&strm);
			printf("Error in Reading file %s\n", source);
			close(readFd);
			close(writeFd);
            return;
        }
		
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
				printf("Error in ZLIB: Memory issue.\n");
                (void)inflateEnd(&strm);
				close(readFd);
				close(writeFd);
                return;
            }
            numBytes = CHUNK - strm.avail_out;
			write(writeFd, out, numBytes);
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
	
	close(readFd);
	close(writeFd);
}


// decompresses project numBytes:<content> to .project

static void decompressProject(int sock, char *projectFile, char *baseDir) {
    /*
	SocketBuffer *socketBuffer = createBuffer();
	readTillDelimiter(socketBuffer, sockFd, ':');
	char *numBytesStr = readAllBuffer(socketBuffer);
	long numBytes = atol(numBytesStr);*/
    SocketBuffer* socketBuffer = createBuffer();
    readTillDelimiter(socketBuffer, sock, ':');
    readTillDelimiter(socketBuffer, sock, ':');
    char* lenCompressedFileName = readAllBuffer(socketBuffer);

    readTillDelimiter(socketBuffer, sock, ':');
    char* compressedFileName = readAllBuffer(socketBuffer);

    readTillDelimiter(socketBuffer, sock, ':');
    char* compressedFileSize = readAllBuffer(socketBuffer);
    long int cfSize = atol(compressedFileSize);

    readNBytes(socketBuffer, sock, cfSize);
    char* compressedFileContent = readAllBuffer(socketBuffer);
    char* pathToCompressedFile = (char*)(malloc(sizeof(char) * (strlen(baseDir) + 1 + strlen(compressedFileName))));
    sprintf(pathToCompressedFile, "%s/%s", baseDir, compressedFileName);
    int compressedFileFD = open(pathToCompressedFile, O_CREAT | O_WRONLY, 00777);
    write(compressedFileFD, compressedFileContent, cfSize);
    close(compressedFileFD);
	
	// now unecrypt data from this file, and write to .project file.
	decompressFile(pathToCompressedFile, projectFile);
	
	// delete temp file.
	unlink(pathToCompressedFile);
	
	//free(numBytesStr);
	freeSocketBuffer(socketBuffer);
}



// writes sent project in this format - <contentLen>:<compressed data>:...
//filePath: path to a file we want to compress
//.projectC is temporary
static void compressProject(int sockFd, char *filePath, char *baseDir) {
	//baseDir: serverRepos/<projName>
	char *pathToCompressedFile = malloc(sizeof(char) * (strlen(baseDir) + strlen("/.projectC")));		
	
	// convert .project file to zlib compressed.
	sprintf(pathToCompressedFile, "%s/.projectC", baseDir);
	
	compressFile(filePath, pathToCompressedFile);
	long int compressedFileSize = findFileSize(pathToCompressedFile);
    long int compressedFileNameLength = strlen(".projectC");
	
	int readFd = open(pathToCompressedFile, O_RDONLY, 00777);
	
    char* fileInfo = (char*)(malloc(sizeof(char) * (11 + 1 + strlen(".projectC") + 1 + 11)));
	sprintf(fileInfo, "%ld:.projectC:%ld:", compressedFileNameLength, compressedFileSize);
	write(sockFd, fileInfo, strlen(fileInfo));
	
	// now write compressed data to socket.
	writeNBytesToFile(compressedFileSize, readFd, sockFd);
	
	close(readFd);
	//unlink(pathToCompressedFile);
	free(pathToCompressedFile);
}

#endif
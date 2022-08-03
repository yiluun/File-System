/**************************************************************
* Class:  CSC-415-01  Summer 2022
* Names: Melody Ku, Matthew Lee, Daniel Guo, Christopher Yee
* Student IDs: 921647126, 918428763, 913290045, 916230255
* GitHub Name: harimku, Mattlee0610, yiluun, JoJoBuffalo
* Group Name: Bug Catchers
* Project: Basic File System
*
* File: b_io.c
*
* Description: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "mfs.h"
#include "fsLow.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	// The ones under here are added to the given fcb struct
	int fileAccessMode;	// 0: read only	1: write only	2: read write
	off_t offset;		// where in the buffer you are
	int fileSize;		// size of the whole file
	long startingLoc;		// location of starting block
	char *pathname;		// stores path to change location later
	int dirtyBufferFlag; //Checks if the buffer is dirty
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open (char * filename, int flags)
	{
	b_io_fd returnFd;
	if (startup == 0) b_init();  //Initialize our system

	parsePathStruct *pathStruct = parsePath(filename);
	flagStruct *givenFlagStruct = giveFlags(flags);
	// It is a valid path and the path is a directory, so it can’t be a file
	if (pathStruct->isValidPath == 1 && pathStruct->isDirectory == 'd') {return -1;}
	
	int isFile = pathStruct->isValidPath;		// default 0; 1 is file, 0 is not a file->dir or not exist

	// initializing variables for simpler access of values
	int isRDONLY = givenFlagStruct->isRDONLY;
	int isWRONLY = givenFlagStruct->isWRONLY;
	int isRDWR = givenFlagStruct->isRDWR;
	int isCreate = givenFlagStruct->isCreate;
	int isTrunc = givenFlagStruct->isTrunc;

	// We will add the flags together; if it is equal to 1, that means we are given the correct amount of
	// flags, else we return an error.
	int accessFlagAmount = isRDONLY + isWRONLY + isRDWR; 
	if (accessFlagAmount != 1) {return -1;}

	// error detection if have isRDONLY and also passed isCreate and/or isTrunc
	if (isRDONLY && (isCreate || isTrunc)) {return -1;}

	/*** This is where we create, trunc and get the file entry index ***/
	int fileIndex;
	DEntry *parentDir = pathStruct->dirEntry;
	int parentDirLen = parentDir[0].size / sizeof(DEntry);
	if (isCreate || (isCreate && isTrunc)) {
		if (isFile) {return -1;}		// can’t create a file if one already exists
		// isCreate only, or both isCreate and isTrunc
		fileIndex = setFileEntry(parentDir, pathStruct->lastToken);
		if (fileIndex == -1) {return -1;}	// failed to create file entry
		parentDir[fileIndex].size = 0;	// trunc size to 0
	} else if (isTrunc && !(isCreate && isTrunc)) {
		if (!isFile) {return -1;}		// can’t truncate a non-existing file
		// isTrunc only, only valid when the file exists already
		for (int i = 0; i < parentDirLen; i++) {
			if (strcmp(parentDir[i].fileName, pathStruct->lastToken) == 0) {
				parentDir[i].size = 0;	// trunc size to 0
				fileIndex = i;
				break;
			}
		}
	} else {
		if (!isFile) {return -1;}		// can’t open a file that doesn’t exist and won’t be created
		// else, none of isCreate and isTrunc, only valid when there’s a file to be opened
		for (int i = 0; i < parentDirLen; i++) {
			if (strcmp(parentDir[i].fileName, pathStruct->lastToken) == 0) {
				fileIndex = i;
				break;
			}
		}
	}
	
	returnFd = b_getFCB();				// get our own file descriptor
	if (returnFd == -1) {return -1;}	// check for error - all used FCB's
	
	/*** Opening the file, specified at fileIndex ***/
	// getting the correct flag
	int flagValue;
	if (isRDONLY) {flagValue = O_RDONLY;}
	if (isWRONLY) {flagValue = O_WRONLY;}
	if (isRDWR) {flagValue = O_RDWR;}

	int sizeOfBlock = vcbPtr->sizeOfBlock;
	fcbArray[returnFd].buf = malloc(sizeOfBlock);		// giving buf a malloc memory
	fcbArray[returnFd].index = 0;				// none of the buffer has been filled yet
	fcbArray[returnFd].buflen = sizeOfBlock;			// size of buffer
	fcbArray[returnFd].fileAccessMode = flagValue;		// access mode
	fcbArray[returnFd].offset = 0;				// offset start at 0, beginning of file
	fcbArray[returnFd].fileSize = parentDir[fileIndex].size;
	fcbArray[returnFd].startingLoc = parentDir[fileIndex].location;
	fcbArray[returnFd].pathname = filename;		// stores path to change locations later

	return (returnFd);						// all set
	}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	if (whence != (0 || 1 || 2)) {return -1;}	// if the whence isn’t one of the valid ones, return -1
	b_fcb givenFile = fcbArray[fd];				// gives the fcb of the file at the fd

	off_t tempOffset = givenFile.offset;
	switch (whence) {
		case 0:			// SEEK_SET
			tempOffset = offset;
			break;
		case 1:			// SEEK_CUR
			tempOffset = tempOffset + offset;
			break;
		case 2:			// SEEK_END
			tempOffset = givenFile.fileSize + offset;
			break;
			
	}
	if (tempOffset < 0) {return -1;} 		// if the final value of the offset is less than 0, return error
	givenFile.offset = tempOffset;		// sets the offset of the fcb to the tempOffset
	return (givenFile.offset);
	}



// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	b_fcb givenFile = fcbArray[fd];				// gives the fcb of the file at the fd
	if (givenFile.fileAccessMode == 0) {return -1;}		// file is read only, can’t write
	if (count < 0) {return -1;}		                    // given a negative count
	
    int bytesWritten;
    int bufferIndex;
    int fileIndex;
	int bufRemain;

    char * fileBuf = givenFile.buf;
    int bufferSize = givenFile.buflen;
    int blockSize = vcbPtr->sizeOfBlock;
    int fileSize = givenFile.fileSize;
    int fileBlockAmt = (fileSize + blockSize - 1) / blockSize;
    int offsetLeft = givenFile.offset % blockSize;
    int blockOffset = givenFile.offset / blockSize;

    // |________Current File Size_______|
    // |___Offset_____|_________count_______| = size of the new file
    int newFileSize = givenFile.offset + count;
    int newBlockAmt = (newFileSize + blockSize - 1) / blockSize;

    long startBlock = givenFile.startingLoc;
    long newStartBlock;

    if (newBlockAmt > fileBlockAmt) {
        /*
        * The file after being written to would be larger than the current space available.
        * We will first move the file to a larger location before writing the buffer into disk.
        */
        // find a new starting block
        newStartBlock = findFreeBlock(newBlockAmt);
        for (int a = 0; a < newBlockAmt; a++) {
            // allocating new blocks in bitmap
            allocateBlock(newStartBlock + a);
        }
        for (int i = 0; i < fileBlockAmt; i++) {
            // loop through old blocks, read them into buffer and then write to disk
            // offsets, indexes and sizes of the file are the same as before the move.
            // they wouldn't need any changes/updates here
            LBAread(fileBuf, 1, startBlock + i);
            LBAwrite(fileBuf,1, newStartBlock + i);
            freeBlock(startBlock + i);
        }
        changeLocation(givenFile.pathname, newStartBlock, newFileSize);  // changes the block loc in DE
        startBlock = newStartBlock;         // keeps track of the start block the file is on
    }
    /*** Being here means you either didn't have to move the file or it has finished moving ***/
    // Now it is time to write the given buffer to the file
    if (offsetLeft != 0) {
        // There is an offset from a full block, so a block is partially filled; fill it up first
        bufRemain = bufferSize - offsetLeft;
        // If they amount they are trying to write is less than how much the buffer can hold
        // only write their count
        if (count < bufRemain) {bufRemain = count;}
        // read it into buffer from disk and memmove their data over
        LBAread(fileBuf, 1, startBlock + blockOffset);
        memmove(fileBuf + offsetLeft, buffer, bufRemain);   // move enough to fill buffer
        // buffer is now filled all the way
        LBAwrite(fileBuf, 1, startBlock + blockOffset);     // write it back in
		fcbArray[fd].dirtyBufferFlag = 1; //Set our flag to 1 so we know we have a dirty buffer
        // Update tracking values
        givenFile.offset += bufRemain;          // should be now even block amount
        count -= bufRemain;                     // count - how much got moved
        givenFile.index = 0;                    // buffer index now at beginning
        bytesWritten += bufRemain;              // written bufRemain bytes to disk
    }

    // now the file on disk is an even amount of blocks, unless there wasn't enough bytes written
    while (1) {
        fileIndex = givenFile.index;
        if (count == 0) {break;}                // nothing else to write
        /* don't need to check if file has enough space after file offset because it was
        * calculated when looking to see if the file needed to be moved
        * move a whole buffer size worth of data into file buffer, we can write
		* directly full blocks because if there was any partially filled blocks in disk, it 
		* would have been filled by the pervious if statement
		*/
		if (count < bufferSize) {
			// if the count is not enough to fill the buffer
			memmove(fileBuf, buffer + bytesWritten, count);
			// write the count into disk at the start + an offset amount
			LBAwrite(fileBuf, 1, startBlock + blockOffset);
			fcbArray[fd].dirtyBufferFlag = 1; //Set our flag to 1 so we know we have a dirty buffer
			// update values
			givenFile.index = count;
			givenFile.offset += count;
			bytesWritten += count;
			givenFile.fileSize += count;
			count = 0;
			break;			// no more bytes to write
		} else {
			// count is more than the buffer size, we can copy full blocks from count
			memmove(fileBuf, buffer + bytesWritten, bufferSize);
        	count -= bufferSize;
        	givenFile.index += bufferSize;      // buffer now full
			givenFile.offset += bufferSize;		// offset by full block
			blockOffset = givenFile.offset / blockSize;	// should be next empty block
			// writing to next block, should be a fully empty one by now
			LBAwrite(fileBuf, 1, startBlock + blockOffset);
			bytesWritten += bufferSize;			// we wrote a whole buffer size into disk
			givenFile.fileSize += bufferSize;	// file size increased by buffer size
			fcbArray[fd].dirtyBufferFlag = 1; //Set our flag to 1 so we know we have a dirty buffer
		}
    }
	return bytesWritten;
	}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	b_fcb givenFile = fcbArray[fd];				// gives the fcb of the file at the fd
	if (givenFile.fileAccessMode == 1) {return -1;}		// file is write only, can’t read
	if (count < 0) {return -1;}		// given a negative count

	int bytesRead;
	int fileIndex;
	int blockOffset;
	int offsetLeft;
	int bufferSize = givenFile.buflen;
	long startBlock = givenFile.startingLoc;
	int bufRemain = bufferSize - fileIndex;
    int fileSizeLeft = givenFile.fileSize - givenFile.offset;

	while(1) {
		if (count == 0) {break;}	// count of 0

		fileIndex = givenFile.index;
		blockOffset = givenFile.offset / vcbPtr->sizeOfBlock;
		if (blockOffset > (givenFile.fileSize + vcbPtr->sizeOfBlock - 1)/vcbPtr->sizeOfBlock) {
			// offset out of file blocks
			break;
		}

		if (fileIndex == 0) {
			// LBAread a block to the file buffer
			LBAread(givenFile.buf, bufferSize, startBlock+blockOffset);
			offsetLeft = givenFile.offset % vcbPtr->sizeOfBlock;
			fileIndex += offsetLeft;
			bufRemain -= offsetLeft;
		}

        // end edge case: they request more than the file has
        if (count > fileSizeLeft && bufferSize > fileSizeLeft) {
            // There's less bytes in the file than the bufferSize, and the count is larger than
            // bytes left. So they are requesting for more bytes than the file has, return the 
            // remainder of the file and then break out of loop.
            memmove(buffer, givenFile.buf + fileIndex, fileSizeLeft);
            givenFile.index += fileSizeLeft;
            count -= fileSizeLeft;
            bytesRead += fileSizeLeft; 
            givenFile.offset += fileSizeLeft;      // should be equal to file size
            break;
        }

		if (count > bufRemain) {		// count larger than remaining space
			memmove(buffer, givenFile.buf + fileIndex, bufRemain);
			givenFile.index = 0;
			givenFile.offset += bufRemain;
			bytesRead += bufRemain;
			count -= bufRemain;
			
		} else {				// count is smaller than remaining space
			memmove(buffer, givenFile.buf + fileIndex, count);
			givenFile.index += count;
			givenFile.offset += count;
			bytesRead += count;
			count = 0;
		}
		bufRemain = bufferSize - givenFile.index;
	}
	return bytesRead;
	}


// Interface to Close the file	
int b_close (b_io_fd fd) {
	// Closes the file; the file descriptor no longer refers to the file and can be used again by another file
	//Get the remaining bytes
	int remainingBufferBytes = fcbArray[fd].buflen - fcbArray[fd].index;

	if(fcbArray[fd].dirtyBufferFlag == 1){ //Check to see if buffer is still dirty (still has unread data)
		b_write(fd, fcbArray[fd].buf, remainingBufferBytes);
	}
	//Free the buffer
	free(fcbArray[fd].buf);
	fcbArray[fd].buf = NULL;
	return 0;	
}



/*** HELPER FUNCTIONS ***/

// This is the new function to see what flags were given, it’s based off of the values given by the fcntl.h header file
flagStruct *giveFlags (int flags) {
	flagStruct *returnStruct = malloc(sizeof(flagStruct));
	/*
	* This for loop goes through each bit of the given flag by shifting the bits left and right to 
	* allow us to get only a single bit at a time. We are assuming that we only get the O_RDONLY
	* flag if both the O_WRONLY and O_RDWR flags aren't given.
	*/
	for (int i = 0; i < 10; i++) {
		int currentFlag = (flags & ( 1 << i )) >> i;
		switch (i) {
			case 0:
				returnStruct->isWRONLY = currentFlag;
				break;
			case 1:
				returnStruct->isRDWR = currentFlag;
				break;
			case 6:
				returnStruct->isCreate = currentFlag;
				break;
			case 9:
				returnStruct->isTrunc = currentFlag;
				break;
		}
	}
    if ((returnStruct->isWRONLY || returnStruct->isRDWR) == 0) {
		returnStruct->isRDONLY = 1;	// assumes: only happens is isWRONLY and isRDWR are 0
	} else {
		returnStruct->isRDONLY = 0;
	}
	return returnStruct;
}


int setFileEntry(DEntry *parentDir, char *fileName) {
	// init a file to start with only one block
	int initFileBlockSize = 1;
	int bytesNeeded = initFileBlockSize * vcbPtr->sizeOfBlock;
	long startingBlock = findFreeBlock(initFileBlockSize);
	int dirLen = parentDir[0].size / sizeof(DEntry);
	int newFileIndex;

	// Sets an empty DE in the parent to serve as a file entry
	for (int i = 0; i < dirLen; i++) {
		// set a first found empty entry and exit loop
		if (strcmp(parentDir[i].fileName, "") != 0) {
			strcpy(parentDir[i].fileName, fileName);
			parentDir[i].location = startingBlock;	// location of the start of the file
			parentDir[i].isDirectory = '_';		// setting the DENtry is file
			parentDir[i].size = bytesNeeded;
			parentDir[i].createdTime = time(NULL);
			parentDir[i].modifiedTime = time(NULL);
			parentDir[i].identifier = 304;

			newFileIndex = i;
			break;
		}
		
		if ((i+1) == dirLen) { return -1; }	// no more free entries
	}

	// If an entry is set, rewrite the parentDir blocks back to disk 
	long parentDirBlocks = (parentDir[0].size + vcbPtr->sizeOfBlock -1)/ vcbPtr->sizeOfBlock;
	LBAwrite(parentDir, parentDirBlocks, parentDir[0].location);

	// Mark the new file's block as allocated & return the set entry index
	allocateBlock(startingBlock);
	return (newFileIndex);
}


void changeLocation(char *path, long newLocBlock, int newSize) {
	parsePathStruct *pathStruct = parsePath(path);
	DEntry * parentDir = pathStruct->dirEntry;
	int dirLen = parentDir[0].size / sizeof(DEntry);

	for (int i = 0; i < dirLen; i++) {
		if (strcmp(parentDir[i].fileName, pathStruct->lastToken) == 0) {
			parentDir[i].location = newLocBlock;
			parentDir[i].size = newSize;
			break;
		}
	}
	
}

/*** example 1 ***/
//	|_____|_____|_____|_____|_____|_____|_____|_____|_____|		|_____| = 100
//	|_____|_____|___|											offset of 260
//	offset/blockSize -> 260/100 = 2		last block is start + offsetBlocks
//	offset%blockSize -> 260%100 = 60	offset of that last block is 60

/*** example 2 ***/
//	|_____|_____|_____|_____|_____|_____|_____|_____|_____|		|_____| = 100
//	|_____|_____|_____|											offset of 300
//	offset/blockSize -> 300/100 = 3		last block is start + offsetBlocks
//	offset%blockSize -> 300%100 = 0		offset of that last block is 0
//	full empty block at start + offsetBlocks

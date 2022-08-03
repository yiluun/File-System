/**************************************************************
* Class:  CSC-415-01 Fall 2021
* Names: Melody Ku, Daniel Guo, Matthew Lee, Christopher Yee
* Student IDs: 921647126, 913290045, 918428763, 916230255
* GitHub Name: harimku, yiluun, Mattlee0610, JoJoBuffalo
* Group Name: Bug Catchers
* Project: Basic File System
*
* File: fsInit.c
*
* Description: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"
#include "drprintf.h"


#define MAGIC_NUM  123456


// Declare global variables
long SystemSize;      // 
struct VCB * vcbPtr;  // Make global pointer to our VCB
char * freeSpaceMap;  // pointer to our free space map to keep during program
long freeSpaceMapBlockCount;  // # of blocks free space map occupies
//long freeSpaceMapLength;  // number of bytes 


// sets the bit representing the block at given index as 0
void allocateBlock(long index) {
    long byteLoc = index / 8;
    freeSpaceMap[byteLoc] = freeSpaceMap[byteLoc] & (~(1 << (index % 8)));
    LBAwrite(freeSpaceMap, freeSpaceMapBlockCount, 1);  // Save this update to disk  
}
 

// sets the bit representing the block at given index as 1, freeing it for use
void freeBlock(long index) {
    long byteLoc = index / 8;
    freeSpaceMap[byteLoc] = freeSpaceMap[byteLoc] | (1 << (index % 8));
    LBAwrite(freeSpaceMap, freeSpaceMapBlockCount, 1);  // Save this update to disk  
}


// Search for a given number of  contiguous free blocks and return starting index
long findFreeBlock(long numberOfBlocks) {
    long count = 0;  // temp counter
    long bestFitSoFar = -1;  // keep track of the best fit size
    long bestFitStartingIndex; // keep track of the index at which the best fit starts
	unsigned int bit = 0;

	// Iterate through every bit of the bit map & find the group of free blocks
	// that best match the specified number of blocks
	// Using systemsize to iterate as there are equal number of bits in the 
	// bit map as the number of blocks in the system.
    for (int i = 0; i < SystemSize; i++) {
		// find the byte index for ith bit
        long byteLoc = i / 8;

		// set every bit other than the bit we want to look at as 1
        bit = (freeSpaceMap[byteLoc] | ~(1 << (i % 8)));

        // if every bit is 1 (FF-->-1), we know the bit we want is also 1 (1 = free)
        if (bit == -1) {
            count++;
		} else { // else we know the bit we want is 0 (0 = allocated)
            if (count > numberOfBlocks) {
                if (bestFitSoFar == -1 || count < bestFitSoFar) {
                    bestFitSoFar = count;
                    bestFitStartingIndex = i - numberOfBlocks;
				}
			}

			// whether we update the best fit hole or not, we reset
            // the count variable for when we start measuring the next hole
            count = 0;
		}
    }

    // If the last group of free blocks & is a better fit, update variables
    if (count > numberOfBlocks) {
		if (bestFitSoFar != -1 && count < bestFitSoFar) {
			bestFitSoFar = count;
			bestFitStartingIndex = SystemSize - count;
		} else if (bestFitSoFar == -1) {
			bestFitSoFar = count;
			bestFitStartingIndex = SystemSize - count;
		}
    }

	// Return the starting index of the best fit hole
    if (bestFitSoFar == -1) {
        return -1; // return -1 if no fit was found
    } else {
        return bestFitStartingIndex;
    }
}


// Function that initializes a free space map
long initFreeSpaceMap(long numberOfBlocks, int blockSize) {

	// Number of bytes we need to represent the total number of blocks.
	// if we have 9 blocks --> we need 9 bits --> 2 bytes needed
    long freeSpaceMapLength = (numberOfBlocks + 7) / 8;

	// Number of blocks needed to store the free space map
    freeSpaceMapBlockCount = (freeSpaceMapLength + blockSize - 1) / blockSize;

	// Allocate calculates bytes for our free space map
	freeSpaceMap = malloc(freeSpaceMapBlockCount * blockSize);

	// Mark block 0(VCB) and blocks containing the free space map as 0
	for (int i = 0; i < freeSpaceMapBlockCount+2; i++) {
		allocateBlock(i);
	}

	// Mark the rest of the free space map to be free (1)
	for (int i = freeSpaceMapBlockCount+2; i < numberOfBlocks; i++) {
		freeBlock(i);
	}

	// Save this free space map to disk 
	LBAwrite(freeSpaceMap, freeSpaceMapBlockCount, 2);

	// Return block #2                   
	return 2;
}


DEntry * createDir(DEntry * destDir, char * newDirName) {
	
	// Malloc enough memory for a new directory
	int bytesNeeded = vcbPtr->directoryLength * sizeof(DEntry);
	int numBlocksNeeded = (bytesNeeded + (vcbPtr->sizeOfBlock-1)) / vcbPtr->sizeOfBlock;
	DEntry * dEntryArray = malloc(numBlocksNeeded * vcbPtr->sizeOfBlock);

	// Find free space
	long startingBlock = findFreeBlock(numBlocksNeeded);
	
	// Set "." entry
	strcpy(dEntryArray[0].fileName, ".");
    dEntryArray[0].isDirectory = 'd';
    dEntryArray[0].size = bytesNeeded;
    dEntryArray[0].location = startingBlock;
    dEntryArray[0].createdTime = time(NULL);
    dEntryArray[0].modifiedTime = time(NULL);
	dEntryArray[0].identifier = 6;

    // Set ".." entry
	strcpy(dEntryArray[1].fileName, "..");
	strcpy(dEntryArray[1].owner, destDir[0].owner);
    dEntryArray[1].isDirectory = destDir[0].isDirectory;
    dEntryArray[1].size = destDir[0].size;
    dEntryArray[1].location = destDir[0].location;
    dEntryArray[1].createdTime = destDir[0].createdTime;
    dEntryArray[1].modifiedTime = destDir[0].modifiedTime;
    dEntryArray[1].identifier = destDir[0].identifier;

	// Write the newly made directory (its entries) in disk & update free space map
    LBAwrite(dEntryArray, numBlocksNeeded, startingBlock);
	for (int i = startingBlock; i < startingBlock+numBlocksNeeded; i++) {
		allocateBlock(i);
	}

	// Now, we must set an entry in parent directory for this new sub directory
	int dirLen = destDir[0].size / sizeof(DEntry);
	DEntry * returnEntry;

	// iterate through entry array(parent directory) and set first available entry
	for (int i = 0; i < dirLen; i++) {
		// set a first found empty entry and exit loop
		if (destDir[i].fileName == NULL) {
			strcpy(destDir[i].fileName, newDirName);
			destDir[i].location = startingBlock;
			destDir[i].isDirectory = 'd';
			destDir[i].size = bytesNeeded;
			destDir[i].createdTime = dEntryArray[0].createdTime;
			destDir[i].modifiedTime = dEntryArray[0].modifiedTime;
			destDir[i].identifier = 6;

			returnEntry = &destDir[i];
			break;
		}
	}

	// Write this change in parent directory back to disk
	LBAwrite(destDir, numBlocksNeeded, destDir[0].location);

	// return the entry of newly written directory
    return returnEntry;
}


// Function that initializes root directory 
long initRootDir(int blockSize) {

	// Create a pointer to an array of directory entries
	int numDEntry = vcbPtr->directoryLength;
    int bytesNeeded = numDEntry * sizeof(DEntry);  // 50 * 104 bytes = 5200 bytes
    int numBlocksNeeded = (bytesNeeded + (blockSize-1)) / blockSize;
	DEntry * dEntryArray = malloc(numBlocksNeeded * blockSize);
    
	// Ask free space map for 6 blocks
    long startingBlock = findFreeBlock(numBlocksNeeded);

	// Set "." entry
	strncpy(dEntryArray[0].fileName, ".", 1);
	strncpy(dEntryArray[0].owner, "system", 1);
    dEntryArray[0].isDirectory = 'd';
    dEntryArray[0].size = numDEntry * sizeof(DEntry);
    dEntryArray[0].location = startingBlock;
    dEntryArray[0].createdTime = time(NULL);
    dEntryArray[0].modifiedTime = time(NULL);
	dEntryArray[0].identifier = 3;

    // Set ".." entry
	strcpy(dEntryArray[1].fileName, "..");
	strcpy(dEntryArray[1].owner, "system");
    dEntryArray[1].isDirectory = 'd';
    dEntryArray[1].size = numDEntry * sizeof(DEntry); // bytes needed
    dEntryArray[1].location = startingBlock;
    dEntryArray[1].createdTime = time(NULL);
    dEntryArray[1].modifiedTime = time(NULL);
    dEntryArray[1].identifier = 3;

/*
	// print the array for checking
	printf("Iterating the root dir dEntryArray:\n");
    for (int i = 0; i < numDEntry; i++) {
		printf("de %d - filename is : %s\n\n", i, dEntryArray[i].fileName);
	}
    printf("num blocks for root dir: %d\n", numBlocksNeeded);
*/

	printf("root dir start at: %ld\n", startingBlock);

    // Write the root directory (its entries) in disk 
    LBAwrite(dEntryArray, numBlocksNeeded, startingBlock);

	// Mark these blocks as allocated
	for (int i = startingBlock; i < startingBlock+numBlocksNeeded; i++) {
		allocateBlock(i);
	}

	// sets the cwd and root dir to root after init of root
	cwdPtr = &dEntryArray[0];
	rootDirPtr = &dEntryArray[0];
	// return the block # of root directory
    return startingBlock;
}


int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
	
	/* TODO: Add any code you need to initialize your file system. */
	SystemSize = numberOfBlocks; // in blocks

	//struct VCB * vcbPtr = malloc(blockSize);  // Allocate 1 block for our VCB
	vcbPtr = malloc(blockSize);  // Allocate 1 block for our VCB
	LBAread(vcbPtr, 1, 1);  // read block 0 into this VCB pointer

	
	// if signature does not match our initialized volume, we need to initialize this VCB
	if (vcbPtr->magicNum != MAGIC_NUM) {
	
		vcbPtr->numBlocks =  numberOfBlocks; // total block count at start
		vcbPtr->numFreeBlocks =  numberOfBlocks; // total block count at start
		vcbPtr->magicNum = MAGIC_NUM;
		vcbPtr->directoryLength = 50;
		
		// Initialize free space:
		// return value is the block # where fsm is stored!
		// we use this to access our fsm throughout execution
		long fsm = initFreeSpaceMap(numberOfBlocks, blockSize);

		// Initialize Root directory
		long rd = initRootDir(blockSize);
	
        // Set the values from the above calls in our VCB
        vcbPtr->freeSpaceMap = fsm;
        vcbPtr->rootDirectory = rd;
        vcbPtr->firstBlock = findFreeBlock(1);
        vcbPtr->sizeOfBlock = blockSize;
		strncpy(vcbPtr->volumeName, "VCB", 3);

		// LBAWrite this VCB to block 0!
		// write 1 block starting at block 0 (this block is set as allocated inside initFreeSpaceMap)
		LBAwrite(vcbPtr, 1, 0);
	}

	// Hard coded a directory and entries to test
	initTestDir(vcbPtr->sizeOfBlock);
	cwdName = "/";
	//fs_setcwd("/");
	printf("next available block is: %ld\n", findFreeBlock(1));

	printf("Show Top 10 Root Dir Entries\n");
	for (int i=0;i<10; i++){
		printf("Root Dir Entry %d: %s\n", i, rootDirPtr[i].fileName);
	}
	
	return 0;
	}


void exitFileSystem ()
	{
	printf ("System exiting\n");
	}






/*** Testing a creation of directory ***/
/*
* This is hard coding a directory and some entries in it to test the change working directory
* command. This also hardcodes the cwdPtr to test parsePath on it. Disable this in the 
* initFileSystem function.
*/
void initTestDir(int blockSize) {
	printf("\nMaking testing directory\n");
	// Create a pointer to an array of directory entries
	int numDEntry = vcbPtr->directoryLength;
    int bytesNeeded = numDEntry * sizeof(DEntry);  // 50 * 104 bytes = 5200 bytes
    int numBlocksNeeded = (bytesNeeded + (blockSize-1)) / blockSize;
	DEntry * dEntryArray = malloc(numBlocksNeeded * blockSize);
    
	// Ask free space map for 6 blocks
    long startingBlock = findFreeBlock(numBlocksNeeded);

	// Set "." entry
	strncpy(dEntryArray[0].fileName, ".", 1);
	strcpy(dEntryArray[0].owner, "matt");
    dEntryArray[0].isDirectory = 'd';
    dEntryArray[0].size = numDEntry * sizeof(DEntry);
    dEntryArray[0].location = startingBlock;
    dEntryArray[0].createdTime = time(NULL);
    dEntryArray[0].modifiedTime = time(NULL);
	dEntryArray[0].identifier = 3;

    // Set ".." entry
	strcpy(dEntryArray[1].fileName, "..");
	strcpy(dEntryArray[1].owner, "matt");
    dEntryArray[1].isDirectory = 'd';
    dEntryArray[1].size = numDEntry * sizeof(DEntry); // bytes needed
    dEntryArray[1].location = startingBlock;
    dEntryArray[1].createdTime = time(NULL);
    dEntryArray[1].modifiedTime = time(NULL);
    dEntryArray[1].identifier = 3;

	// Set "testDir1" entry
	strcpy(dEntryArray[2].fileName, "testDir1");
	strcpy(dEntryArray[2].owner, "matt");
    dEntryArray[2].isDirectory = 'd';
    dEntryArray[2].size = numDEntry * sizeof(DEntry); // bytes needed
    dEntryArray[2].location = startingBlock;
    dEntryArray[2].createdTime = time(NULL);
    dEntryArray[2].modifiedTime = time(NULL);
    dEntryArray[2].identifier = 3;

	// Set "testDir2" entry
	strcpy(dEntryArray[3].fileName, "testDir2");
	strcpy(dEntryArray[3].owner, "matt");
    dEntryArray[3].isDirectory = 'd';
    dEntryArray[3].size = numDEntry * sizeof(DEntry); // bytes needed
    dEntryArray[3].location = startingBlock;
    dEntryArray[3].createdTime = time(NULL);
    dEntryArray[3].modifiedTime = time(NULL);
    dEntryArray[3].identifier = 3;

	// Set "testFile1" entry
	strcpy(dEntryArray[4].fileName, "testFile1");
	strcpy(dEntryArray[4].owner, "matt");
    dEntryArray[4].isDirectory = '_';
    dEntryArray[4].size = numDEntry * sizeof(DEntry); // bytes needed
    dEntryArray[4].location = startingBlock;
    dEntryArray[4].createdTime = time(NULL);
    dEntryArray[4].modifiedTime = time(NULL);
    dEntryArray[4].identifier = 3;

	//setting parent
	strcpy(rootDirPtr[2].fileName, "testingRoot");
	rootDirPtr[2].location = startingBlock;
	rootDirPtr[2].isDirectory = 'd';
	rootDirPtr[2].size = bytesNeeded;
	rootDirPtr[2].createdTime = dEntryArray[0].createdTime;
	rootDirPtr[2].modifiedTime = dEntryArray[0].modifiedTime;
	rootDirPtr[2].identifier = 16;

	printf("root dir start at: %ld\n", startingBlock);

    // Write the root directory (its entries) in disk 
    LBAwrite(dEntryArray, numBlocksNeeded, startingBlock);

	// Mark these blocks as allocated
	for (int i = startingBlock; i < startingBlock+numBlocksNeeded; i++) {
		allocateBlock(i);
	}

	printf("\nHard coding directory test function\n");
	printf("Check cwdPtr before switch\n");
	for (int i=0;i<10;i++) {
		printf("Dir Name: %s\n", cwdPtr[i].fileName);
	}

	printf("\n");
	// hard code setting cwd to test
	cwdPtr = &dEntryArray[0];

	printf("\nTest directory and files\n");
	for (int i=0;i<10;i++) {
		printf("Dir Name: %s\n", cwdPtr[i].fileName);
	}
}
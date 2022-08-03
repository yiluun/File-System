#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"
#include "drprintf.h"


// Declare global variables
DEntry * rootDirPtr;  // Declare pointer to root directory
DEntry * cwdPtr;  // Declare pointer to current working directory
char * cwdName;  // Declare pointer to the name of current working directory

        

// 
int deleteEntry(DEntry * parentDir, int tokenIndex) {
    // set the bits for the given file's blocks as free
    long startingLocation = parentDir[tokenIndex].location;
    long numBlocks = (parentDir[tokenIndex].size + vcbPtr->sizeOfBlock -1)
                    / vcbPtr->sizeOfBlock;
    for (int i = startingLocation; i < startingLocation + numBlocks; i++) {
        freeBlock(i);
    }

    //set this entry to null(empty state) 
    parentDir[tokenIndex].accessTime = time(0);
    parentDir[tokenIndex].createdTime = time(0);
    parentDir[tokenIndex].modifiedTime = time(0);
    parentDir[tokenIndex].isDirectory = ' ';
    parentDir[tokenIndex].location = 0;
    parentDir[tokenIndex].size = 0;
    parentDir[tokenIndex].identifier = 0;
	strcpy(parentDir[tokenIndex].fileName, "");
    strcpy(parentDir[tokenIndex].owner, "");

    // write the modified blocks to disk
    numBlocks = (parentDir[0].size + vcbPtr->sizeOfBlock -1) / vcbPtr->sizeOfBlock;
    LBAwrite(parentDir, parentDir[0].location, numBlocks);

    return 0;
}



DEntry * loadDir(DEntry * target) {

    // read the entire array of directory entries
    int bytes = target->size;
    int numblocks = (bytes + (vcbPtr->sizeOfBlock -1)) / vcbPtr->sizeOfBlock;
    DEntry * buffer = malloc(numblocks * vcbPtr->sizeOfBlock);
    LBAread(buffer, numblocks, target->location);
    return buffer;
}


long findIndexOfToken(DEntry * buffer, char * name, long maxEntries) {
    printf("Find index of token\n");
    printf("name: %s\n", name);
    printf("max entries: %ld\n", maxEntries);
    for (int i = 0; i < maxEntries; i++) {
        printf("Token %d: %s\n", i, buffer[i].fileName);
        if (strcmp(buffer[i].fileName, name) == 0) {
            return i;
        }
    }
    return -1;
}


parsePathStruct * parsePath(char * path) {

    // malloc space for the return structure and populate with default values
    parsePathStruct * returnStruct = malloc(sizeof(parsePathStruct));
    returnStruct->isValidPath = 0;

    // Initializing variables for path tokenizing
    char * token;
    int arrIndex = 0;		// holds the counter index for the parsedDir array
    char * parsedDir[256];		// array which can hold 256 path levels
    int nextLocation;		// used by for loop
    
    // This section parses the given path and stores each part into the parsedDir array.
    char * delim = "/";
    token = strtok(path, delim);

    // If no valid token, exit
    if (token == NULL) {
        return returnStruct; 			
    }
 
    // Initialize variables for parsing
    DEntry * deArrayPtr;  // stores the loaded directory entry
    DEntry * tempPtr;  // temp variable used to free previous loaded entry
    long tokenIndex;  // stores the index of the found entry (matching token)
    int flag = 0;  // declare a flag variable for each token search
    int lastValidIndex = 0;

    printf("Parse Path Given Path: %s\n", path);
    // in case of absolute path, add a self entry to parsed array
    //  & load the root directory as a starting point
    if (path[0] == '/') {
        parsedDir[arrIndex] = ".";
        arrIndex++;

        // initialize a mock DEntry for the root directory
        DEntry de;
        de.size = vcbPtr->directoryLength * sizeof(DEntry);
        de.location = vcbPtr->rootDirectory;
        de.isDirectory = 'd';
        DEntry * mockRootDir = &de;
        printf("Loaded rootPtr\n");
        // load root directory
        deArrayPtr = (rootDirPtr);
    } else {  // load the current working directory otherwise
        printf("Loaded cwdPtr\n");
        deArrayPtr = (cwdPtr);
    }

    // tokenize rest of the given path
    while (token != NULL) {
        parsedDir[arrIndex] = token;
        token = strtok(NULL, delim);
        arrIndex++;
    }

    printf("1\n");
    for (int i = 0; i < arrIndex; i++) {
		/*
		* Loop through an array of directory entries for current directory level.
        * If next token(next path) is found, load that directory, set flag, and
        * break loop. If a file matching token name is found and we have more 
        * tokens to parse, ignore and keep searching for a directory of that name.
		*/
        lastValidIndex = i;
        flag = 0;  // reset flag for new token search
        long maxEntries = deArrayPtr[0].size / sizeof(DEntry);
		tokenIndex = findIndexOfToken(deArrayPtr, parsedDir[i], maxEntries);
       
        if (tokenIndex == -1) { // if no match is found with token
            printf("Token: %s\n", parsedDir[i]);
            printf("Failed \n");
            break;
        } else if (i+1 == arrIndex) { // if we reach the last token
            returnStruct->lastToken = parsedDir[i]; // save the last token
            printf("2\n");
            flag = 1;  // set flag to indicate found
            break;  // break to avoid loading the last token
        }
        
        tempPtr = loadDir(&deArrayPtr[tokenIndex]);
        free(deArrayPtr);
        deArrayPtr = tempPtr;
	}

    // if at this point nothing was found, path is not valid
	if (flag == 0) {
        returnStruct->isValidPath = 0;

		// saving lastValidLevel and nonValidLevels in return struct
        returnStruct->lastValidLevel = deArrayPtr;
        for (int k = 0; k < arrIndex-1-lastValidIndex; k++) {
            returnStruct->nonValidLevels[k] = parsedDir[lastValidIndex+k+1];
        }
        return returnStruct;
    }

	// if we reach this point, we have succeeded in finding the entire path
 	returnStruct->isValidPath = 1;
	returnStruct->isDirectory = deArrayPtr[tokenIndex].isDirectory;
    returnStruct->dirEntry = deArrayPtr;

    // return the returnStruct
    return returnStruct;
}


int fs_isFile(char * path) {	//return 1 if file, 0 otherwise
    parsePathStruct * pathStruct = parsePath(path);

    if (pathStruct->isValidPath != 1) {
        free(pathStruct);
        return -1;
    } else {
        if (pathStruct->isDirectory == '_') {
            free(pathStruct);
            return 1;
        } else {
            free(pathStruct);
            return 0;
        }
    }
}


int fs_isDir(char * path) { //return 1 if directory, 0 otherwise
    parsePathStruct * pathStruct = parsePath(path);

    if (pathStruct->isValidPath != 1) {
        free(pathStruct);
        return -1;
    } else {
        if (pathStruct->isDirectory == 'd') {
            free(pathStruct);
            return 1;
        } else {
            free(pathStruct);
            return 0;
        }
    }
}


// Helper function for concatenating the current path and the last path element
char * concatPath(char * currentPath, char * lastElement) {
    int currentPathLen = strlen(currentPath);
    int lastElementLen = strlen(lastElement);

    // add two for the “/” char and null terminator
    int sizeNeeded = currentPathLen + lastElementLen + 2; 
    char *newPath = malloc(sizeNeeded);

    // Append the strings
    strcpy(newPath, currentPath);
    strcat(newPath, "/");		// adds the “/”
    strcat(newPath, lastElement);
    return newPath;
}


int fs_setcwd(char * buf) {
    // parse given path and get info on path
    parsePathStruct * pathStruct = parsePath(buf);

    
    // exit if path is invalid
    if (pathStruct->isValidPath == 0) {
        return -1;
    }

    // This section sets the entry of current working directory
    DEntry * parentDir = pathStruct->dirEntry;
    long maxEntries = parentDir->size / sizeof(DEntry);
	int tokenIndex = findIndexOfToken(parentDir, pathStruct->lastToken, maxEntries);
    if (cwdPtr != NULL) {  // if cwdPtr has existing entry stored, free that
        free(cwdPtr);
    }

    // Set cwdPtr (global variable) to the found entry
    cwdPtr = &parentDir[tokenIndex];

/*
    printf("reached this point in setcwd!!\n");
    printf("pathStruct values:  \n isDirectory: %d\n", cwdPtr->isDirectory);
    printf("fileName: %s\n", cwdPtr->fileName);
    printf("last path token:  %s", pathStruct->lastToken);
*/

	// This section changes the path of current working directory (global variable)
	cwdName = concatPath(cwdName, pathStruct->lastToken);
	return 0;
}


char * fs_getcwd(char * buf, size_t size) {
    strncpy(buf, cwdName, size);
    return buf;
}


int fs_mkdir(const char * pathname, mode_t mode) {
    // Parse the given path
    parsePathStruct * parsedStruct = parsePath((char *)pathname);
	
    // exit if path is already a valid directory path
	if (parsedStruct->isValidPath == 1) {
        return -1;
    }

	// at this point, either the path isn’t valid or the last element is not
    // an existing directory; load the last valid directory entry level recorded
    // in parsePath. 
	DEntry * loadedDir = loadDir(parsedStruct->lastValidLevel);
	DEntry * lastCreatedDir;
	int arrLen = sizeof(parsedStruct->nonValidLevels) / sizeof(char *);


//printf("loaded dir: %s", loadedDir[0].fileName);

	for (int i = 0; i < arrLen; i++) {
        // Create a new directory in the loaded directory
        lastCreatedDir = createDir(loadedDir, parsedStruct->nonValidLevels[i]);

        // Load the newly made directory for next token
        loadedDir = loadDir(lastCreatedDir);
    }
    printf("Directory size: %ld\n", loadedDir[0].size);

    free(parsedStruct);
    free(loadedDir);
    parsedStruct = NULL;
    loadedDir = NULL;
    printf("mkDir finished\n");
    return 0;	// mkDir was a success
}


int fs_rmdir(const char * pathname) {
    parsePathStruct * parsedStruct = parsePath((char *)pathname);

    // exit if path is invalid or is a file
    if (parsedStruct->isValidPath == 0 || parsedStruct->isDirectory == '_') {
        return -1;
    } else {  // if such path exists
        // load the parent then the final directory
        DEntry * parentDir = loadDir(parsedStruct->dirEntry);
        long maxEntries = parentDir[0].size / sizeof(DEntry);
        int tokenIndex = findIndexOfToken(parentDir, parsedStruct->lastToken, maxEntries);
        DEntry * toDeleteDir = loadDir(&parentDir[tokenIndex]);
        long numBlocks = (toDeleteDir[0].size + vcbPtr->sizeOfBlock - 1) / vcbPtr->sizeOfBlock;

        // iterate through the final directory and search for any valid entry
        for (int i = 2; i < maxEntries; i++) {
            if (strcmp(toDeleteDir[i].fileName, "") != 0) {
                return -1;  // exit if directory is not empty
            }
        }

        // Delete the entry and its blocks in disk
        int ret = deleteEntry(parentDir, tokenIndex);
        return ret;
    }
}


int fs_delete(char * filename) {
    parsePathStruct * tempStruct = parsePath(filename);
    
    // error if path is invalid or not a file
    if (tempStruct->isValidPath == 0 || tempStruct->isDirectory == 'd') {
        return -1;
    }

    // if path is a file
    DEntry * parentDir = loadDir(tempStruct->dirEntry);

    // iterate through parent directory to find the DEntry of the file
    long maxEntries = parentDir[0].size / sizeof(DEntry);
    int tokenIndex = findIndexOfToken(parentDir, tempStruct->lastToken, maxEntries);

    // free the blocks & delete the entry using utility function
    int ret = deleteEntry(parentDir, tokenIndex);
    return ret;
}


int fs_stat(const char * path, struct fs_stat * buf) {
    parsePathStruct * parsedStruct = parsePath((char *)path);
    if (parsedStruct->isValidPath == 0 ) {
        return -1;
    };

    DEntry * parentDir = loadDir(parsedStruct->dirEntry);
    long maxEntries = parentDir[0].size / sizeof(DEntry);
    int tokenIndex = findIndexOfToken(parentDir, parsedStruct->lastToken, maxEntries);

    buf->st_blksize = vcbPtr->sizeOfBlock;
    buf->st_blocks = (parentDir[tokenIndex].size + vcbPtr->sizeOfBlock -1) /
    vcbPtr->sizeOfBlock;
    buf->st_size = parentDir[tokenIndex].size; 
    buf-> st_accesstime = parentDir[tokenIndex].accessTime;
    buf->st_modtime = parentDir[tokenIndex].modifiedTime;
    buf->st_createtime = parentDir[tokenIndex].createdTime;
    buf->st_isDirectory = parentDir[tokenIndex].isDirectory;
    strcpy(&buf->st_fileName, parentDir[tokenIndex].fileName);

    return 0;
}


fdDir * fs_opendir(const char * name) {
    fdDir * fdd = malloc(sizeof(fdDir));
    parsePathStruct * parsedStruct = parsePath((char *)name);

    // if path is invalid or is not a directory, return an empty struct
    if (parsedStruct->isValidPath == 0 || parsedStruct->isDirectory == '_') {
        fdd->isValid = 0;
        return fdd;
    }

    DEntry * parentDir = loadDir(parsedStruct->dirEntry);
    long maxEntries = parentDir[0].size / sizeof(DEntry);
    int tokenIndex = findIndexOfToken(parentDir, parsedStruct->lastToken, maxEntries);
    DEntry * toOpen = &parentDir[tokenIndex];

    fdd->isValid = 1;
    fdd->d_reclen = (toOpen->size) / sizeof(DEntry);
    fdd->dirEntryPosition = 0;  // readdir should read from the 0th entry
    fdd->directoryStartLocation = toOpen->location;
    strcpy(fdd->dirItemInfo.d_name, toOpen->fileName);
    fdd->dirItemInfo.d_reclen = toOpen->size / sizeof(DEntry);
    fdd->dirItemInfo.fileType = 'd';
    fdd->dirPtr = toOpen;

    return fdd;
}


struct fs_diriteminfo * fs_readdir(fdDir * dirp) {

    if (dirp->isValid == 1 && dirp->dirPtr->isDirectory == 'd') {
        // If path is valid, load the directory and look for the next valid entry
        DEntry * dirArray = loadDir(dirp->dirPtr);
    
        for (int i = dirp->dirEntryPosition; i < dirp->d_reclen; i++) {
            if (strcmp(dirArray[i].fileName, "") != 0) {
                strcpy(dirp->dirItemInfo.d_name, dirArray[i].fileName);
                dirp->dirItemInfo.d_reclen = dirArray[i].size / sizeof(DEntry);
                dirp->dirItemInfo.fileType = dirArray[i].isDirectory;
                dirp->dirEntryPosition = i+1;  // update the next entry position
                return (&dirp->dirItemInfo);
            }
        }
        strcpy(dirp->dirItemInfo.d_name, "");
        dirp->dirItemInfo.d_reclen = 0;
        dirp->dirItemInfo.fileType = ' ';
        dirp->dirEntryPosition = dirp->d_reclen+1;
        return (&dirp->dirItemInfo);

    } else {
        strcpy(dirp->dirItemInfo.d_name, "");
        dirp->dirItemInfo.d_reclen = 0;
        dirp->dirItemInfo.fileType = ' ';
        dirp->dirEntryPosition = 0;
        return (&dirp->dirItemInfo);
    }
}


int fs_closedir(fdDir *dirp) {
    // frees and closes all memory associated with the given entry
    if (dirp == NULL || dirp->isValid == 0) {
        return -1;
    }

    free(dirp->dirPtr);
    dirp->dirPtr = NULL;
    free(dirp);
    dirp = NULL;
    return 0;
}

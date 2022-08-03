/**************************************************************
* Class:  CSC-415-01  Summer 2022
* Name: Professor Bierman
* Student ID: N/A
* Project: Basic File System
*
* File: mfs.h
*
* Description: 
*	This is the file system interface.
*	This is the interface needed by the driver to interact with
*	your filesystem.
*
**************************************************************/
#ifndef _MFS_H
#define _MFS_H
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "b_io.h"
#include "fsLow.h"

#include <dirent.h>
#define FT_REGFILE	DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK	DT_LNK

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif



// File system structs

typedef struct VCB {
	long numBlocks;  // total number of blocks for our system
	long numFreeBlocks;    // number of free blocks
	long freeSpaceMap;    // to find the free space bitmap 
	long magicNum;    // special signature 
	long firstBlock;    // this marks the first block of the volume
	long rootDirectory; 	// location of root dir (this is a block #)
	int directoryLength;  // we will create 50 entries per directory
	int sizeOfBlock;
	char volumeName[30];
} VCB;

typedef struct DEntry {
	long identifier;
	long size;
	time_t createdTime; // time that it was created
	time_t modifiedTime;  // last time the directory/file was modified
	time_t accessTime;  // last access time
	int location;
	char isDirectory;  // ‘d’ = directory, ‘_’ for file
	char fileName[30];
	char owner[30]; // owner of the specific directory/file
} DEntry;


// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo {
	unsigned short d_reclen;    /* length of this record */
	unsigned char fileType;    
	char d_name[256]; 			/* filename max filename is 255 characters */
};


// This is a private structure used only by fs_opendir, fs_readdir, and fs_closedir
// Think of this like a file descriptor but for a directory - one can only read
// from a directory.  This structure helps you (the file system) keep track of
// which directory entry you are currently processing so that everytime the caller
// calls the function readdir, you give the next entry in the directory
typedef struct {
	/*****TO DO:  Fill in this structure with what your open/read directory needs  *****/
	unsigned short  d_reclen;		/*length of this record */
	unsigned short	dirEntryPosition;	/*which directory entry position, like file pos */
	uint64_t	directoryStartLocation;		/*Starting LBA of directory */
	
	int isValid;
	struct fs_diriteminfo dirItemInfo;
	DEntry * dirPtr;
} fdDir;


// structure that is returned by parsePath, contains some info
typedef struct  {
	int isValidPath;		// 1 = valid, 0 = not valid
	char isDirectory; 	// d = directory, _ = file
	DEntry * dirEntry; 	// the parent directory
	DEntry * lastValidLevel;		// store ptr to last valid level
	char * nonValidLevels[256];	// store tokens of each invalid path level
	char * lastToken;  // value of last token in the path, if path is valid
} parsePathStruct;


typedef struct flagStruct {
	int isRDONLY;		// 1 is a given flag, 0 is not given
	int isWRONLY;		// 1 is a given flag, 0 is not given
	int isRDWR;		// 1 is a given flag, 0 is not given
	int isCreate;		// 1 is a given flag, 0 is not given
	int isTrunc;		// 1 is a given flag, 0 is not given
} flagStruct;


// extern declaration of global variables
extern long SystemSize;      // total number of blocks in system
							 // blockSize can be accessed via vcbPtr
extern struct VCB * vcbPtr;  // global pointer to our VCB
extern char * freeSpaceMap;  // pointer to our free space map
extern DEntry * rootDirPtr;  // pointer to root directory
extern DEntry * cwdPtr;      // pointer to current working directory
extern char * cwdName;   // name of current working directory


// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);

// Directory iteration functions
fdDir * fs_opendir(const char *name);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char * fs_getcwd(char *buf, size_t size);
int fs_setcwd(char *buf);   //linux chdir
int fs_isFile(char * path);	//return 1 if file, 0 otherwise
int fs_isDir(char * path);		//return 1 if directory, 0 otherwise
int fs_delete(char* filename);	//removes a file

// Utility functions
parsePathStruct * parsePath(char * path);
int deleteEntry(DEntry * parent, int i);
DEntry * loadDir(DEntry * target);
DEntry * createDir(DEntry * destDir, char * newDirName);
long findFreeBlock(long numberOfBlocks);
void allocateBlock(long index);
void freeBlock(long index);
flagStruct * giveFlags (int flags);
int setFileEntry(DEntry * parentDir, char * fileName);
void changeLocation(char *path, long newLocBlock, int newSize);

void initTestDir(int blockSize);


// This is the strucutre that is filled in from a call to fs_stat
struct fs_stat
	{
	off_t     st_size;    		/* total size, in bytes */
	blksize_t st_blksize; 		/* blocksize for file system I/O */
	blkcnt_t  st_blocks;  		/* number of 512B blocks allocated */
	time_t    st_accesstime;   	/* time of last access */
	time_t    st_modtime;   	/* time of last modification */
	time_t    st_createtime;   	/* time of last status change */
	
	/* add additional attributes here for your file system */
	char st_isDirectory;
    char st_fileName;
	};

int fs_stat(const char *path, struct fs_stat *buf);


#endif
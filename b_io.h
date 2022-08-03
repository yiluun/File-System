/**************************************************************
* Class:  CSC-415-01  Summer 2022
* Names: Melody Ku, Matthew Lee, Daniel Guo, Christopher Yee
* Student IDs: 921647126, 918428763, 913290045, 916230255
* GitHub Name: harimku, Mattlee0610, yiluun, JoJoBuffalo
* Group Name: Bug Catchers
* Project: Basic File System
*
* File: b_io.h
*
* Description: Interface of basic I/O functions
*
**************************************************************/

#ifndef _B_IO_H
#define _B_IO_H
#include <fcntl.h>

#include "fsLow.h"
#include "mfs.h"

typedef int b_io_fd;

b_io_fd b_open (char * filename, int flags);
int b_read (b_io_fd fd, char * buffer, int count);
int b_write (b_io_fd fd, char * buffer, int count);
int b_seek (b_io_fd fd, off_t offset, int whence);
int b_close (b_io_fd fd);


#endif
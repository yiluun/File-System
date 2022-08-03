/**************************************************************
* Class:  CSC-415-0# Spring 2022
* Name:  Robert Bierman
* Student ID: 
* GitHub ID: 
* Project: Any
*
* File: dprintf.h
*
* Description: defines drintf as a macro
**************************************************************/

#include <stdio.h>


// The purpose of this macro os to provide a version of printf that can be
// controlled via a #define, specifically DEBUG
// You use this just like you would with printf, i.e.
//		dprintf ("This is my debug statement, x = %d and y = %d\n", x, y);
//
// In addition to the normal print, it will also display the file name,
// the function, and the line number
//
// When DEBUG is not defined, then dprintf does nothing and no print will occur
//
// To define debug place the following line in your source code file prior to the
// include statement: #include "dprintf.h"
// #define DEBUG

// To turn off dprintf, just comment out the line #define DEBUG



#ifdef DEBUG
    #define dprintf(fmt, args...) printf("%s:%s:%d: "fmt, __FILE__, __FUNCTION__,__LINE__, args)                                                                          
#else
    #define dprintf(fmt, args...) {}
#endif


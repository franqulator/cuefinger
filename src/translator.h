/*
This file is part of Cuefinger 1

Cuefinger 1 gives you the possibility to remote control Universal Audio's
Console Application via Network (TCP).
Copyright © 2024 Frank Brempel

Cuefinger 1 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef _TRANSLATOR_H_
#define _TRANSLATOR_H_

#include <stdio.h>
#include <cstring>
#include <time.h>
#ifdef __ANDROID__
	#include <android_fopen.h>
#endif

//Portabilität Übersetzer
#define ZeroMemory(ptr,len) (memset(ptr,0,len))
#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

#define GetRValue(rgb) ((unsigned char)rgb)
#define GetGValue(rgb) ((unsigned char)(rgb >> 8))
#define GetBValue(rgb) ((unsigned char)(rgb >> 16))
#define RGB(r,g,b)      ((COLORREF)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned int)(unsigned char)(b))<<16)))

struct _finddata_t
{
	unsigned attrib; /* File attributes */
	long time_create; /* -1L for FAT file systems */
	long time_access; /* -1L for FAT file systems */
	long time_write; /* Time of last modification */
	size_t size; /* Size of file in bytes */
	char name[256]; /* Name of file witout complete path*/
};

typedef unsigned int COLORREF;
typedef int errno_t;
unsigned long long GetTickCount64();
errno_t fopen_s(FILE **f, const char *name, const char *mode);
errno_t _itoa_s(int value, char *buffer, size_t size, int radix);
errno_t memcpy_s(void *dest, size_t destsz, const void *src, size_t count);
errno_t strcpy_s(char *dest, size_t destsz, const char *src);
errno_t strncpy_s(char *dest, size_t destsz, const char *src, size_t count);
errno_t strcat_s(char *dest, size_t destsz, const char *src);
char *strtok_s(char *str, const char *delim, char **ptr);
int _stricmp(const char *string1, const char *string2);
void Sleep(unsigned int ms);
errno_t localtime_s(tm* tmDest,time_t* sourceTime);
errno_t _splitpath_s(const char * path,char * drive,size_t driveNumberOfElements,
   char * dir,size_t dirNumberOfElements,char * fname,size_t nameNumberOfElements,char * ext,size_t extNumberOfElements);
#endif

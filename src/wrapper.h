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

#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#ifdef __linux__ 
	#include <dirent.h>
#elif _WIN32
	#include <io.h>
	#include <direct.h>
#endif

#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#ifdef __linux__ 
class FIND_FILE
{
private:
    DIR *stream;
    char path[256];
    char ext[8];
 public:
    char name[256];
    tm last_modified;
    FIND_FILE();
    ~FIND_FILE();
	bool first(const char *_path, const char *_ext);
    bool next();    
};

#elif _WIN32

class FIND_FILE
{
private:
    intptr_t stream;
public:
    char name[256];
    tm last_modified;
    FIND_FILE();
    ~FIND_FILE();
	bool first(const char *path, const char *_ext);
    bool next();    
};

#endif


bool create_dir(const char *path);

#endif
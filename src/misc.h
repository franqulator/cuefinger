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

#ifndef _MISC_H_
#define _MISC_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <map>
#include <set>
#include <stdexcept>
#include "wrapper.h"
#ifdef __ANDROID__
    #include <android/log.h>
#endif								  
using namespace std;

#ifdef __linux__ 
	#include "translator.h"
	#define PATH_SEPERATOR '/'
#elif _WIN32
	#include <windows.h>
	#define PATH_SEPERATOR '\\'
#endif


class Rect
{
public:
	int left;
	int top;
	int right;
	int bottom;

	Rect(int left=0, int top=0, int right=0, int bottom=0);
	void set(int left, int top, int right, int bottom);
	int getWidth();
	int getHeight();
};

struct SmallRect
{
	short Left;
	short Top;
	short Right;
	short Bottom;
};

struct Size
{
	int cx;
	int cy;
};

struct Point
{
	int x;
	int y;
};

void initDataPath(string _datafolder); // readonly
string getDataPath(string subpath);

void initPrefPath(string _preffolder); // write and read
string getPrefPath(string subpath);

void initAppPath(string _appfolder);
string getAppPath(string subpath);

void clearLog(string info);
void toLog(string str);

class LibraryFile {
public:
	size_t size;
	unsigned char *buffer;
	set<string> iniSections;
	map<string, string> iniPairs;
	LibraryFile(unsigned int size, unsigned char *buffer);
	LibraryFile(string filepath);
	~LibraryFile();
	bool parseIni();
	bool hasIniSection(string _section);
	int getIniInt(string _section, string _key, int _default);
	string getIniString(string _section, string _key, string _default);
};

class Library {
private:
	map<string, LibraryFile*> libraryFiles;
public:
	Library(string libPath);
	~Library();
	LibraryFile* getFile(string filename);
};

bool ptInRect(::Rect *rc, ::Point pt);
string trim(string s);

#endif
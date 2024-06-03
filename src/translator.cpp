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
#include "translator.h"

#ifdef __linux_wsl__
	#include "../include/SDL.h"
#elif __linux__ 
	#ifdef __ANDROID__
		#include <SDL.h>
	#else
		#include <SDL2/SDL.h>
	#endif
#endif

#include <strings.h>
#include <libgen.h>
#include <string>

//Portabilität übersetzerfunktionen
unsigned long long GetTickCount64()
{
#if SDL_VERSION_ATLEAST(2,0,18) 
    return SDL_GetTicks64();
#else
	return SDL_GetTicks();
#endif
}

errno_t fopen_s(FILE **f, const char *name, const char *mode)
{
    *f = fopen(name, mode);
    if (!*f)
        return 1;
    return 0;
}

errno_t _itoa_s(int value, char *buffer, size_t size, int radix)
{
	if(!SDL_itoa(value, buffer,radix))
		return 1;

	return 0;
}

errno_t memcpy_s(void *dest, size_t destsz, const void *src, size_t count)
{
	if(!memcpy(dest,src,count))
		return 1;
	return 0;
}

errno_t strcpy_s(char *dest, size_t destsz, const char *src)
{
	strcpy(dest,src);
	return 0;
}

errno_t strncpy_s(char *dest, size_t destsz, const char *src, size_t count)
{
	strncpy(dest,src,count);
	return 0;
}

errno_t strcat_s(char *dest, size_t destsz, const char *src)
{
	strcat(dest,src);
	return 0;
}

char *strtok_s(char *str, const char *delim, char **ptr)
{
	char *ret = strtok(str, delim);
	if(ptr && ret)
	{
		ret+=strlen(ret)+1;
		*ptr = ret;
	}
	return ret;
}

int _stricmp(const char *string1, const char *string2)
{
	return strcasecmp(string1, string2);
}

void Sleep(unsigned int ms)
{
	SDL_Delay(ms);
}

errno_t localtime_s(tm* tmDest,time_t *sourceTime)
{
	tmDest = localtime(sourceTime);
	return 0;
}


errno_t _splitpath_s(const char * _path,char * drive,size_t driveNumberOfElements,
   char * dir,size_t dirNumberOfElements,char * fname,size_t nameNumberOfElements,char * ext,size_t extNumberOfElements)
{
	std::string path;
	std::string filename;
	std::string extension;

	path = _path;
	
	size_t pos = path.find_last_of("\\/");
	if(pos != std::string::npos)
	{
		filename = path.substr(pos+1);
		path = path.substr(0,pos + 1);
	}
	else
	{
		filename = path;
		path = "";
	}
	
	pos = filename.find_last_of('.');
	if(pos != std::string::npos)
	{
		extension = filename.substr(pos);
		filename = filename.substr(0,pos);
	}
	else
	{
		extension = "";
		path = "";
	}
	
	if(drive)
	{
		strcpy(drive,"");
	}
	if(dir)
	{
		strcpy(dir, path.c_str());
	}
	if(fname)
	{
		strcpy(fname, filename.c_str());
	}
	if(ext)
	{
		strcpy(ext, extension.c_str());
	}
	
	return 0;
}
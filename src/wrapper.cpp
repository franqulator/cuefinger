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

#include "wrapper.h"

FIND_FILE::FIND_FILE()
{
	memset(this, 0, sizeof(FIND_FILE));
}

#ifdef __linux__ 

FIND_FILE::~FIND_FILE()
{
    if(this->stream)
    {
        closedir(this->stream);
    }
}
bool FIND_FILE::first(const char *_path, const char *_ext)
{
	this->name[0] = 0;
	strcpy(this->path, _path);
	strcpy(this->ext, ".");
	strcat(this->ext, _ext);
	this->stream = opendir(path);
	if (this->stream)
	{
		return this->next();
	}

	return false;
}
bool FIND_FILE::next()
{
    this->name[0] = 0;
    if(this->stream)
    {
        dirent *dp;
        do
        {
            dp = readdir(this->stream);
            if(dp)
            {
                if(strstr(dp->d_name,this->ext))
                {
                    strcpy(this->name, dp->d_name);
                    char fullpath[512];
                    sprintf(fullpath, "%s/%s", this->path, this->name);
                    struct stat attrib;
                    stat(fullpath, &attrib);
                    tm *p = gmtime(&attrib.st_mtime);
                    memcpy(&this->last_modified, p, sizeof(tm));
                    return true;
                }
            }
        }while(dp);
    }
    return false;
}

bool create_dir(const char *path)
{
	return (mkdir(path, S_IRWXU) == 0);
}

#elif _WIN32

FIND_FILE::~FIND_FILE()
{
    if(this->stream)
    {
        _findclose(this->stream);
    }
}
bool FIND_FILE::first(const char *_path, const char *_ext)
{
	char path[256];
	strcpy_s(path, 256, _path);
	strcat_s(path, 256, "*.");
	strcat_s(path, 256, _ext);
	this->name[0] = 0;
	_finddata_t fi;
	this->stream = _findfirst(path, &fi);
	if (this->stream != -1)
	{
		strcpy_s(this->name, 256, fi.name);
		localtime_s(&this->last_modified, &fi.time_write);
		return true;
	}
	return false;
}
bool FIND_FILE::next()
{
    this->name[0] = 0;
    if(this->stream)
    {
        _finddata_t fi;
        if(_findnext(this->stream,&fi)==0)
        {
            strcpy_s(this->name, 256, fi.name);
            localtime_s(&this->last_modified, &fi.time_write);
            return true;
        }
    }
    return false;
}

bool create_dir(const char *path)
{
	return (_mkdir(path) == 0);
}

#endif
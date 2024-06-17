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

#include "misc.h"

//weitere funktionen
string g_dataFolder="";
void initDataPath(string dataFolder) { // read only
	g_dataFolder = dataFolder;
}

string getDataPath(string subpath) { // folder to read
	string str(g_dataFolder);
	str += subpath; 

#ifdef __linux__ 
	for (size_t n = 0; n < str.length(); n++) {
		if (str[n] == '\\') {
			str[n] = '/';
		}
	}
#endif

	return str;
}

string g_prefFolder = "";
void initPrefPath(string prefFolder) { // read and write
	g_prefFolder = prefFolder;
}

string getPrefPath(string subpath) { // folder to read and write
	string str(g_prefFolder);
	str += subpath;

#ifdef __linux__ 
	for (size_t n = 0; n < str.length(); n++) {
		if (str[n] == '\\') {
			str[n] = '/';
		}
	}
#endif

	return str;
}

string g_appFolder = "";
void initAppPath(string appfolder) {// only read
	g_appFolder = appfolder;
}

string getAppPath(string subpath) {
	string str(g_appFolder);
	str += subpath; 
	
#ifdef __linux__ 
	for(size_t n = 0; n < str.length(); n++) {
		if(str[n] == '\\') {
			str[n] = '/';
		}
	}
#endif

	return str;
}

void initLog(const string &info) {
#ifndef __ANDROID__				   
	time_t tme = time(NULL);
	tm t;
	errno_t err = localtime_s(&t, &tme);

	string path = getPrefPath("cuefinger.log");

	FILE *fh = NULL;
	if (fopen_s(&fh, path.c_str(), "w") == 0) {
		string str = info + "\ncuefiner.log from ";

		if (err == 0) {
			str += to_string(t.tm_year + 1900);
			str += "-";
			str += to_string(t.tm_mon + 1);
			str += "-";
			str += to_string(t.tm_mday);
			str += ", ";
			str += to_string(t.tm_hour);
			str += ":";
			str += to_string(t.tm_min);
		}
		else {
			str += "localtime error";
		}
		str += "\n---\n";
		fputs(str.c_str(), fh);
		fclose(fh);
	}
#endif	  
}
void toLog(const string &str) {
#ifndef __ANDROID__			  															   
	string path;
	path = getPrefPath("cuefinger.log");

	FILE *fh = NULL;
	if (fopen_s(&fh, path.c_str(), "a") == 0) {
		fputs(str.c_str(), fh);
		fputs("\n", fh);
		fclose(fh);
	}
#endif	  
}


string iso_8859_1_to_utf8(string &str) {
    string strOut = "";
    for (string::iterator it = str.begin(); it != str.end(); ++it) {
        unsigned char ch = *it;
        if (ch < 0x80) {
            strOut.push_back(ch);
        }
        else {
            strOut.push_back(0xc0 | ch >> 6);
            strOut.push_back(0x80 | (ch & 0x3f));
        }
    }
    return strOut;
}

string trim(string &s) {
	// trim left
	while (s.length() > 0 && (s[0]==' ' || s[0] == '\t' || s[0] == '\r' || s[0] == '\n')) {
		s.erase(0, 1);
	}
	// trim right
	while (s.length() > 0 && (s[s.length()-1]==' ' || s[s.length()-1] == '\t' || s[s.length()-1] == '\r' || s[s.length()-1] == '\n')) {
		s.erase(s.length()-1);
	}
	return s;
}

unsigned int sgets(unsigned char *buffer, unsigned int ptr, unsigned int buffer_size, char *line, unsigned int len) {
	if (!buffer || !line)
		return 0;

	for (unsigned int n = 0; ; n++)	{
		if (n >= len-1 || n>= buffer_size - ptr) {
			line[n] = '\0';
			return n;
		}

		line[n] = buffer[ptr + n];
		if (buffer[ptr + n] == '\n' || buffer[ptr + n]=='\0') {
			line[n+1] = '\0';
			return n+1;
		}
	}
	return 0;
}

bool LibraryFile::parseIni() {
	if (this->iniPairs.empty()) { // bei erster Anfrage Ini pharsen

		if (!this->buffer) {
			return false;
		}

		string curSection = "";
		char line[4096];

		//parse for section
		unsigned int read = 0;
		int p = 0;
		while (true) {
			read = sgets(this->buffer, p, (unsigned int)this->size, line, 4096);
			p += read;
			if (read < 1)
				break;

			char* comment = NULL;
			comment = strtok_s(line, "#", &comment); // kommentar abschneiden

			string sLine(reinterpret_cast<char*>(line));
			trim(sLine);

			if (sLine.length() > 2 && sLine[0] == '[' && sLine[sLine.length() - 1] == ']') {
				curSection = sLine.substr(1, sLine.length() - 2);
				iso_8859_1_to_utf8(curSection);
				this->iniSections.insert(curSection);
			}
			else if(sLine.length() > 0 && sLine[0] != '#') {
				size_t split = sLine.find_first_of('=');
				if (split != string::npos) {
					string key = sLine.substr(0, split);
					trim(key);
					key = curSection + "->" + iso_8859_1_to_utf8(key);

					string value = sLine.substr(split + 1);
					trim(value);
					iso_8859_1_to_utf8(value);

					this->iniPairs.insert(pair<string, string>(key, value));
				}
			}
		}
	}
	return true;
}

bool LibraryFile::hasIniSection(string _section) {
	if (!this->parseIni()) {
		return false;
	}
	return this->iniSections.find(_section) != this->iniSections.end();
}

string LibraryFile::getIniString(string _section, string _key, string _default) {
	string _value = _default;

	if (!this->parseIni()) {
		return _value;
	}

	// aus map lesen
	map<string, string>::iterator it = this->iniPairs.find(_section + "->" + _key);
	if (it != this->iniPairs.end()) {
		_value = this->iniPairs[_section + "->" + _key];
	}

	return _value;
}

int LibraryFile::getIniInt(string _section, string _key, int _default) {
	string _value_str;
	_value_str = this->getIniString(_section, _key, to_string(_default));

	return stoi(_value_str, NULL, 10);
}

LibraryFile::LibraryFile(string filepath) {
	FILE *f;
	if (fopen_s(&f, filepath.c_str(), "rb") != 0) {
		throw invalid_argument("Fehler beim öffnen der Datei.");
	}

	if (fseek(f, 0, SEEK_END)) {
		throw invalid_argument("Fehler beim lesen der Datei: fseek failed");
	}
	this->size = ftell(f);

	if (fseek(f, 0, SEEK_SET)) {
		throw invalid_argument("Fehler beim lesen der Datei: fseek failed");
	}

	this->buffer = new unsigned char[this->size];
	size_t result = fread(this->buffer, sizeof(unsigned char), this->size, f);
	if (result != this->size) {
		throw invalid_argument("Fehler beim lesen der Datei: fread returned wrong size");
	}

	fclose(f);
}

LibraryFile::LibraryFile(unsigned int size, unsigned char *buffer) {
	this->size = size;
	this->buffer = buffer;
}

LibraryFile::~LibraryFile() {
	if (this->buffer) {
		delete[] this->buffer;
		this->buffer = NULL;
	}
	this->size = 0;
}

Library::Library(string libPath) {

	string fullLibPath = getAppPath(libPath);

	FILE *f;
	if (fopen_s(&f, fullLibPath.c_str(), "rb") != 0) {
		throw invalid_argument("Die Library konnte nicht geladen werden.");
	}

	int sz = 0;
	if (fread(&sz, sizeof(int), 1, f) != 1) {
		throw invalid_argument("Fehler beim Lesen der Library.");
	}

	long dataPos = 0;

	for (int i = 0; i < sz; i++) {
		int str_len = 0;
		unsigned int size;
		unsigned char filename[256];
		if(fread(&str_len, sizeof(int), 1, f) != 1) {
			throw invalid_argument("Fehler beim Lesen der Library.");
		}
		if (str_len < 256) {
			if (fread(&filename, sizeof(unsigned char), str_len, f) != (size_t)str_len) {
				throw invalid_argument("Fehler beim Lesen der Library.");
			}
			filename[str_len] = '\0';
		}
		else {
			throw invalid_argument("Fehler beim Lesen der Library.");
		}
		
		if(fread(&size, sizeof(unsigned int), 1, f) != 1) {
			throw invalid_argument("Fehler beim Lesen der Library.");
		}

		string sFilename(reinterpret_cast<char*>(filename));

		// zu den daten springen
		long fileTablePos = ftell(f);

		if(!dataPos) { // ersten Datenblock suchen
			for(int n = i + 1; n < sz; n++) {
				if(fread(&str_len, sizeof(int), 1, f) != 1) {
					throw invalid_argument("Fehler beim Lesen der Library.");
				}
				fseek(f, str_len + sizeof(unsigned int), SEEK_CUR);
			}
		}
		else
			fseek(f, dataPos, SEEK_SET);
		unsigned char *buffer = new unsigned char[size];
		if(fread(buffer, sizeof(unsigned char), size, f) != size) {
			throw invalid_argument("Fehler beim Lesen der Library.");
		}
		dataPos = ftell(f);

		LibraryFile *libFile = new LibraryFile(size, buffer);
		this->libraryFiles.insert(pair<string, LibraryFile*>(sFilename, libFile));

		// zum Filetable zurückspringen
		fseek(f, fileTablePos, SEEK_SET);
	}
	fclose(f);
}

Library::~Library() {
	for(map<string, LibraryFile*>::iterator it = this->libraryFiles.begin(); it != this->libraryFiles.end(); it++) {
		if(it->second) {
			delete it->second;
			it->second = NULL;
		}
	}
	this->libraryFiles.clear();
}

LibraryFile* Library::getFile(string filename) {
	map<string, LibraryFile*>::iterator it = this->libraryFiles.find(filename);
	if (it != this->libraryFiles.end()) {
		return this->libraryFiles[filename];
	}
	return NULL;
}

Rect::Rect(int left, int top, int right, int bottom) {
	this->set(left, top, right, bottom);
}

void Rect::set(int left, int top, int right, int bottom) {
	this->left = left;
	this->top = top;
	this->right = right;
	this->bottom = bottom;
}

int Rect::getWidth() {
	return this->right - this->left;
}

int Rect::getHeight() {
	return this->bottom - this->top;
}

bool ptInRect(Rect *rc, Point pt) {
	if (!rc)
		return false;

	return (pt.x >= rc->left && pt.x < rc->right
		&&pt.y >= rc->top && pt.y < rc->bottom);
}
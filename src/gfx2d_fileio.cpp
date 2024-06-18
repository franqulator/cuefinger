//Version 5.1 - 2024-06-03

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

#include "gfx2d_sdl.h"

#define		GFX_24BIT					0x01
#define		GFX_8BIT_PALETTE			0x04
#define		GFX_ALPHA_STRAIGHT			0x10
#define		GFX_ALPHA_PREMULTIPLIED		0x20

#define RGB_565(r,g,b) (((r&248)<<8)|((g&252)<<3)|((b&248)>>3))
#define GETR_565(rgb) ((rgb>>8)&248)
#define GETG_565(rgb) ((rgb>>3)&252)
#define GETB_565(rgb) ((rgb<<3)&248)
#define RGB_555(r,g,b) (((r&248)<<7)|((g&248)<<2)|((b&248)>>3))
#define GETR_555(rgb) ((rgb>>7)&248)
#define GETG_555(rgb) ((rgb>>2)&248)
#define GETB_555(rgb) ((rgb<<3)&248)

bool GFXEngine::_ReadAlphaChannelBmp(GFXSurface *gs, FILE *fh)
{
	char header[3];
	//header
	header[0] = (char)getc(fh);
	header[1] = (char)getc(fh);
	header[2] = '\0';
	if (strcmp(header, "BM") != 0)
	{
		return false;
	}
	//filesize (bytes)
	unsigned int filesize;
	filesize = getc(fh);
	filesize |= getc(fh) << 8;
	filesize |= getc(fh) << 16;
	filesize |= getc(fh) << 24;
	//reserved
	fseek(fh, 4, SEEK_CUR);
	//offset
	unsigned int offset;
	offset = getc(fh);
	offset |= getc(fh) << 8;
	offset |= getc(fh) << 16;
	offset |= getc(fh) << 24;
	//headersize
	fseek(fh, 4, SEEK_CUR);
	//width
	int width;
	width = getc(fh);
	width |= (int)getc(fh) << 8;
	width |= (int)getc(fh) << 16;
	width |= (int)getc(fh) << 24;
	//height
	int height;
	height = getc(fh);
	height |= (int)getc(fh) << 8;
	height |= (int)getc(fh) << 16;
	height |= (int)getc(fh) << 24;

	if (width != gs->w || height != gs->h)
	{
		return false;
	}

	//planes
	unsigned int planes;
	planes = getc(fh);
	planes |= getc(fh) << 8;

	//colordepht
	int colorDepht = getc(fh);
	colorDepht |= (int)getc(fh) << 8;

	//compression
	unsigned int compression;
	compression = getc(fh);
	compression |= (unsigned int)getc(fh) << 8;
	compression |= (unsigned int)getc(fh) << 16;
	compression |= (unsigned int)getc(fh) << 24;
	if (compression && compression != 3)
	{
		return false;
	}

	//bitmapdatasize
	unsigned int bitmapdatasize;
	bitmapdatasize = getc(fh);
	bitmapdatasize |= getc(fh) << 8;
	bitmapdatasize |= getc(fh) << 16;
	bitmapdatasize |= getc(fh) << 24;
	//h-resolution
	fseek(fh, 4, SEEK_CUR);
	//v-resolution
	fseek(fh, 4, SEEK_CUR);
	//colors (palette-size)
	unsigned int palette;
	palette = getc(fh);
	palette |= (unsigned int)getc(fh) << 8;
	palette |= (unsigned int)getc(fh) << 16;
	palette |= (unsigned int)getc(fh) << 24;
	//important colors
	fseek(fh, 4, SEEK_CUR);

	int terminator = 0;
	if ((filesize - offset) > (unsigned int)(width*height*colorDepht / 8))
		terminator = ((int)(filesize - offset) - (width*height*colorDepht / 8)) / height;

	unsigned char *clr = new unsigned char[palette];
	for (unsigned int n = 0; n < palette; n++)
	{
		clr[n] = (unsigned char)(getc(fh) + getc(fh) + getc(fh)) / 3;
		fseek(fh, 1, SEEK_CUR);
	}
	fseek(fh, offset, SEEK_SET);
	unsigned char *p8 = (unsigned char*)gs->bgra;
	switch (colorDepht)
	{
	case 8:
	{
		for (int y = height - 1; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				p8[(y*width + x) * 4 + 3] = clr[getc(fh)];
				if (feof(fh))
					return true;
			}
			fseek(fh, terminator, SEEK_CUR);
		}
		break;
	}
	case 16:
	{
		for (int y = height - 1; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				int v = getc(fh);
				v |= getc(fh) << 8;
				p8[(y*width + x) * 4 + 3] = (unsigned char)((GETR_555(v) + GETG_555(v) + GETB_555(v)) / 3);
				if (feof(fh))
					return true;
			}
			fseek(fh, terminator, SEEK_CUR);
		}
		break;
	}
	case 24:
	{
		for (int y = height - 1; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				p8[(y*width + x) * 4 + 3] = (unsigned char)((getc(fh) + getc(fh) + getc(fh)) / 3);
				if (feof(fh))
					return true;
			}
			fseek(fh, terminator, SEEK_CUR);
		}
		break;
	}
	default:
	{
		return false;
	}
	}
	return true;
}
GFXSurface *GFXEngine::_ReadTga(FILE *fh)
{
	//header
	unsigned char imageID = (unsigned char)getc(fh); //image ID
	unsigned char colorMap = (unsigned char)getc(fh); //color map type
	unsigned char imageType = (unsigned char)getc(fh); //uncompressed truecolor = 2

	if (colorMap)
	{
		return NULL;
	}

	if (imageType != 2)
	{
		return NULL;
	}

	//overjump colormap
	fseek(fh, 5, SEEK_CUR);

	//overjump x/y origin
	fseek(fh, 4, SEEK_CUR);

	int width = getc(fh);
	width |= getc(fh) << 8;
	int height = getc(fh);
	height |= getc(fh) << 8;

	GFXSurface *gsurf = CreateSurface(width, height);
	if (!gsurf)
	{
		return NULL;
	}

	int colorDepht = getc(fh);

	//Image descriptor;
	fseek(fh, 1, SEEK_CUR);

	//imageID field
	fseek(fh, imageID, SEEK_CUR);

	//image map not supported

	switch (colorDepht)
	{
	case 32:
	{
		for (int y = 0; y < height; y++)
		{
			if (fread(&gsurf->bgra[(height - 1 - y) * width], 4, width, fh) != (size_t)width) {
				this->DeleteSurface(gsurf);
				return NULL;
			}
		}
		break;
	}
	default:
	{
		return NULL;
	}
	}
	return gsurf;
}
GFXSurface *GFXEngine::_ReadBmp(FILE *fh)
{
	char header[3];
	//header
	header[0] = (unsigned char)getc(fh);
	header[1] = (unsigned char)getc(fh);
	header[2] = 0;
	if (strcmp(header, "BM") != 0)
	{
		return NULL;
	}
	//filesize (bytes)
	unsigned int filesize;
	filesize = getc(fh);
	filesize |= getc(fh) << 8;
	filesize |= getc(fh) << 16;
	filesize |= getc(fh) << 24;
	//reserved
	fseek(fh, 4, SEEK_CUR);
	//offset
	unsigned int offset;
	offset = getc(fh);
	offset |= getc(fh) << 8;
	offset |= getc(fh) << 16;
	offset |= getc(fh) << 24;
	//headersize
	unsigned int headersize = getc(fh);
	headersize |= getc(fh) << 8;
	headersize |= getc(fh) << 16;
	headersize |= getc(fh) << 24;
	if (headersize != 40)
	{
		return NULL;
	}
	//width
	int width;
	width = getc(fh);
	width |= (unsigned int)getc(fh) << 8;
	width |= (unsigned int)getc(fh) << 16;
	width |= (unsigned int)getc(fh) << 24;
	//height
	int height;
	height = getc(fh);
	height |= (unsigned int)getc(fh) << 8;
	height |= (unsigned int)getc(fh) << 16;
	height |= (unsigned int)getc(fh) << 24;

	//planes
	unsigned int planes;
	planes = getc(fh);
	planes |= getc(fh) << 8;

	GFXSurface *gsurf = CreateSurface(width, height);
	if (!gsurf)
		return NULL;

	EnableAlphaChannel(gsurf, false);

	//colordepht
	unsigned int colorDepht = getc(fh);
	colorDepht |= (unsigned int)getc(fh) << 8;

	//compression
	unsigned int compression;
	compression = getc(fh);
	compression |= (unsigned int)getc(fh) << 8;
	compression |= (unsigned int)getc(fh) << 16;
	compression |= (unsigned int)getc(fh) << 24;
	if (compression && compression != 3)
	{
		return NULL;
	}

	//bitmapdatasize
	unsigned int bitmapdatasize;
	bitmapdatasize = getc(fh);
	bitmapdatasize |= getc(fh) << 8;
	bitmapdatasize |= getc(fh) << 16;
	bitmapdatasize |= getc(fh) << 24;
	//h-resolution
	fseek(fh, 4, SEEK_CUR);
	//v-resolution
	fseek(fh, 4, SEEK_CUR);
	//colors (palette-size)
	unsigned int palette;
	palette = getc(fh);
	palette |= (unsigned int)getc(fh) << 8;
	palette |= (unsigned int)getc(fh) << 16;
	palette |= (unsigned int)getc(fh) << 24;

	if (colorDepht < 16 && !palette)
	{
		palette = 256;
	}
	//important colors
	fseek(fh, 4, SEEK_CUR);

	int terminator = 4 - ((gsurf->w*colorDepht / 8) % 4);
	if (terminator == 4)
		terminator = 0;

	COLORREF *clr = new COLORREF[palette];
	for (unsigned int n = 0; n < palette; n++)
	{
		clr[n] = RGB(getc(fh), getc(fh), getc(fh));
		fseek(fh, 1, SEEK_CUR);
	}
	fseek(fh, offset, SEEK_SET);
	unsigned char *p8 = (unsigned char*)gsurf->bgra;
	switch (colorDepht)
	{
	case 8:
	{
		for (int y = height - 1; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				int index = getc(fh);
				p8[(y*width + x) * 4] = GetBValue(clr[index]);
				p8[(y*width + x) * 4 + 1] = GetGValue(clr[index]);
				p8[(y*width + x) * 4 + 2] = GetRValue(clr[index]);
				p8[(y*width + x) * 4 + 3] = 0xFF;
				if (feof(fh))
					return gsurf;
			}
			fseek(fh, terminator, SEEK_CUR);
		}
		break;
	}
	case 16:
	{
		if (compression == 3) //565
		{
			for (int y = height - 1; y >= 0; y--)
			{
				for (int x = 0; x < width; x++)
				{
					int v = getc(fh);
					v |= getc(fh) << 8;
					p8[(y*width + x) * 4] = (unsigned char)GETB_565(v);
					p8[(y*width + x) * 4 + 1] = (unsigned char)GETG_565(v);
					p8[(y*width + x) * 4 + 2] = (unsigned char)GETR_565(v);
					p8[(y*width + x) * 4 + 3] = 0xFF;
					if (feof(fh))
						return gsurf;
				}
				fseek(fh, terminator, SEEK_CUR);
			}
		}
		else //555
		{
			for (int y = height - 1; y >= 0; y--)
			{
				for (int x = 0; x < width; x++)
				{
					int v = getc(fh);
					v |= getc(fh) << 8;
					p8[(y*width + x) * 4] = (unsigned char)GETB_555(v);
					p8[(y*width + x) * 4 + 1] = (unsigned char)GETG_555(v);
					p8[(y*width + x) * 4 + 2] = (unsigned char)GETR_555(v);
					p8[(y*width + x) * 4 + 3] = 0xFF;
					if (feof(fh))
						return gsurf;
				}
				fseek(fh, terminator, SEEK_CUR);
			}
		}
		break;
	}
	case 24:
	{
		for (int y = height - 1; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				p8[(y*width + x) * 4] = (unsigned char)getc(fh);
				p8[(y*width + x) * 4 + 1] = (unsigned char)getc(fh);
				p8[(y*width + x) * 4 + 2] = (unsigned char)getc(fh);
				p8[(y*width + x) * 4 + 3] = 0xFF;
				if (feof(fh))
					return gsurf;
			}
			fseek(fh, terminator, SEEK_CUR);
		}
		break;
	}
	default:
	{
		return NULL;
	}
	}
	return gsurf;
}

GFXSurface * GFXEngine::LoadGfxFromBuffer(unsigned char *_data, size_t _size)
{
	if (!_data || _size < 18) //header-size + rectcount
		return NULL;

	unsigned char *ptr = _data;
	unsigned char *ptr_end = _data + _size - 1;

	unsigned char header_size = *ptr;
	ptr++;

	char identifier[4];

	memcpy(identifier, ptr, 3);
	ptr += 3;

	identifier[3] = '\0';

	if (strcmp(identifier, "GFX") != 0)
	{
		return NULL;
	}
	unsigned char type = *ptr;
	ptr++;

	unsigned int width = 0, height = 0;
	memcpy(&width, ptr, 4);
	ptr += 4;
	memcpy(&height, ptr, 4);
	ptr += 4;

	GFXSurface *gsurf = CreateSurface(width, height);
	if (!gsurf)
		return NULL;

	bool *used = new bool[gsurf->w*gsurf->h];
	memset(used, 0, sizeof(bool) * gsurf->w * gsurf->h);

	if (type & GFX_ALPHA_STRAIGHT)
	{
		gsurf->use_alpha = true;
		gsurf->alpha_premultiplied = false;
	}
	else if (type & GFX_ALPHA_PREMULTIPLIED)
	{
		gsurf->use_alpha = true;
		gsurf->alpha_premultiplied = true;
	}
	else
	{
		gsurf->use_alpha = false;
		gsurf->alpha_premultiplied = false;
	}

	ptr = &_data[header_size];

	int parts = gsurf->w * gsurf->h; //anzahl der parts für decode2, wird weiter unten fertigberechnet

	if (type & GFX_24BIT)
	{
		//decodestep 1
		unsigned int szRcs = 0;
		memcpy(&szRcs, ptr, 4);
		ptr += 4;

		//check buffer size
		int multiplier = 3 + sizeof(SmallRect);
		if(gsurf->use_alpha)
			multiplier++;
		int needed_size = szRcs * multiplier;
		if (ptr + needed_size >= ptr_end)
		{
			DeleteSurface(gsurf);
			delete[] used;
			return NULL;
		}

		unsigned char *p8 = (unsigned char*)gsurf->bgra;
		unsigned char color[4];

		for (unsigned int n = 0; n < szRcs; n++)
		{
			if (gsurf->use_alpha)
			{
				memcpy(color, ptr, 4);
				ptr += 4;
			}
			else
			{

				memcpy(color, ptr, 3);
                color[3] = 0xFF;
				ptr += 3;
			}

			SmallRect rc;
			memcpy(&rc, ptr, sizeof(SmallRect));
			ptr += sizeof(SmallRect);
			
			if(rc.Top > rc.Bottom || rc.Left > rc.Right ||
				rc.Bottom > gsurf->h || rc.Right > gsurf->w)
			{
				DeleteSurface(gsurf);
				delete[] used;
				return NULL;
			}

			for (int y = rc.Top; y < rc.Bottom; y++)
			{
				for (int x = rc.Left; x < rc.Right; x++)
				{
					if (!used[y*gsurf->w + x])
					{
						memcpy(&gsurf->bgra[y*gsurf->w + x], color, 4);
						used[y*gsurf->w + x] = true;
						parts--;
					}
				}
			}
		}
		//decodestep 2
		int *ids = new int[parts];
		int idx;
		parts = 0;
		for (int i = 0; i < gsurf->w * gsurf->h; i++)
		{
			if (!used[i])
			{
				ids[parts] = i;
				parts++;
			}
		}

		int parts_count = parts / 256;
		if (parts % 256)
			parts_count++;

		for (int n = 0; n < parts_count; n++)
		{
			unsigned char palette_clr_count = *ptr;
			ptr++;

			for (int a = 0; a < palette_clr_count; a++)
			{
				if (gsurf->use_alpha)
				{
					memcpy(color, ptr, 4);
					ptr += 4;
				}
				else
				{
					memcpy(&color, ptr, 3);
					color[3]= 0xFF;
					ptr += 3;
				}

				int nums = (int)(unsigned char)*ptr;
				ptr++;
				if (!nums)
					nums = 256;

				for (int i = 0; i < nums; i++)
				{
					unsigned char pos = *ptr;
					ptr++;

					idx = ids[n * 256 + pos];

					memcpy(&gsurf->bgra[idx], color, 4);
					used[idx] = true;
				}
			}
		}
		delete[] ids;
		ids = NULL;

		//Bitmap
		//array optimized
		for (int i = 0; i < gsurf->h * gsurf->w; i++)
		{
			if (!used[i])
			{
				if (gsurf->use_alpha)
				{
					memcpy(p8, ptr, 4);
					ptr += 4;
				}
				else
				{
					memcpy(p8, ptr, 3);
					p8[3] = 0xFF;
					ptr += 3;
				}
			}
			p8 += 4;
		}
	}
	else if (type & GFX_8BIT_PALETTE)
	{
		unsigned char clr[256][4] = { 0 };
		int pClr = *ptr;
		ptr++;
		if (!pClr)
			return NULL;

		for (int n = 0; n < pClr; n++)
		{
			if (gsurf->use_alpha)
			{
				memcpy(clr[n], ptr, 4);
				ptr += 4;
			}
			else
			{
				memcpy(clr[n], ptr, 3);
				clr[n][3] = 0xFF;
				ptr += 3;
			}
		}

		//decodestep 1
		unsigned int szRcs;
		memcpy(&szRcs, ptr, 4);
		ptr += 4;

		unsigned int color;
		for (unsigned int n = 0; n < szRcs; n++)
		{
			memcpy(&color, clr[*ptr], 4);
			ptr++;

			SmallRect rc;
			memcpy(&rc, ptr, sizeof(SmallRect));
			ptr += sizeof(SmallRect);
			
			if(rc.Top > rc.Bottom || rc.Left > rc.Right ||
				rc.Bottom > gsurf->h || rc.Right > gsurf->w)
			{
				DeleteSurface(gsurf);
				delete[] used;
				return NULL;
			}

			for (int y = rc.Top; y < rc.Bottom; y++)
			{
				for (int x = rc.Left; x < rc.Right; x++)
				{
					gsurf->bgra[y*gsurf->w + x] = color;
					used[y*gsurf->w + x] = true;
				}
			}
		}
		//bitmap
		for (int i = 0; i < gsurf->h * gsurf->w; i++)
		{
			if (!used[i])
			{
				memcpy(&gsurf->bgra[i], clr[*ptr], 4);
				ptr++;
			}
		}
	}
	else
	{
		DeleteSurface(gsurf);
		gsurf = NULL;
	}

	delete[] used;
	used = NULL;

	if (gsurf) {
		EnableAlphaChannel(gsurf, gsurf->use_alpha);
		if (!_CopyColorInfo(gsurf))
		{
			DeleteSurface(gsurf);
			return NULL;
		}
	}

	return gsurf;
}

bool GFXEngine::LoadAlphaChannelBmp(GFXSurface *gs, string name)
{
	if (_HasBGRABuffer(gs)) {
		FILE* fh;
		errno_t e = fopen_s(&fh, name.c_str(), "rb");
		if (e)
			return false;

		bool ret = _ReadAlphaChannelBmp(gs, fh);

		fclose(fh);

		if (!ret) // errorhandling in readalphachannelbmp()
			return false;

		EnableAlphaChannel(gs, true);

		if (!_CopyColorInfo(gs))
		{
			return false;
		}
		return ret;
	}
	return false;
}
GFXSurface* GFXEngine::LoadBmp(string name)
{
	FILE *fh;
	errno_t e = fopen_s(&fh, name.c_str(), "rb");
	if (e)
		return NULL;

	GFXSurface *gsurf = _ReadBmp(fh);
	if (!gsurf)
	{
		//errorhandling in _readbmp()
		fclose(fh);
		return NULL;
	}
	fclose(fh);
	EnableAlphaChannel(gsurf, false);
	if (!_CopyColorInfo(gsurf))
	{
		return NULL;
	}
	return gsurf;
}
GFXSurface* GFXEngine::LoadTga(string name)
{
	FILE *fh;
	errno_t e = fopen_s(&fh, name.c_str(), "rb");
	if (e)
		return NULL;

	GFXSurface *gsurf = _ReadTga(fh);
	if (!gsurf)
	{
		//errorhandling in _readtga()
		fclose(fh);
		return NULL;
	}
	fclose(fh);
	EnableAlphaChannel(gsurf, gsurf->use_alpha);
	if (!_CopyColorInfo(gsurf))
	{
		return NULL;
	}
	return gsurf;
}
GFXSurface* GFXEngine::LoadGfx(string name)
{
	FILE *fh;
	errno_t e = fopen_s(&fh, name.c_str(), "rb");
	if (e)
		return NULL;

	fseek(fh, 0, SEEK_END);
	size_t buffer_sz = ftell(fh);
	unsigned char *data = (unsigned char*)malloc(buffer_sz);
	if (!data)
	{
		fclose(fh);
		return NULL;
	}
	fseek(fh, 0, SEEK_SET);
	buffer_sz = fread(data, sizeof(unsigned char), buffer_sz, fh);
	fclose(fh);

	GFXSurface *gsurf = LoadGfxFromBuffer(data, buffer_sz);
	if (!gsurf)
	{
		//errorhandling in _readpsg
		free(data);
		return NULL;
	}

	free(data);

	return gsurf;
}

bool GFXEngine::SaveGfx(GFXSurface *gs, string name)
{
	if (!gs)
		return false;

	if (_HasBGRABuffer(gs)) {
		bool result = true;
		FILE* fh;
		if (fopen_s(&fh, name.c_str(), "wb") != 0)
		{
			return false;
		}

		//HEADER
		fputc(13, fh);
		fwrite("GFX", 1, 3, fh);

		unsigned int palette_clr[256];
		int palette_sz = 0;
		unsigned char type = GFX_24BIT;

		unsigned int src_clr = 0;
		unsigned char* p8 = (unsigned char*)gs->bgra;

		//check if can be saved as palette
		for (int n = 0; n < gs->w * gs->h; n++)
		{
			if (gs->use_alpha)
				src_clr = gs->bgra[n];
			else
				src_clr = gs->bgra[n] & 0x00FFFFFF;

			if (palette_sz > 255)
				break;
			bool there = false;
			for (int i = 0; i < palette_sz; i++)
			{
				if (src_clr == palette_clr[i])
				{
					there = true;
					break;
				}
			}
			if (!there)
			{
				if (palette_sz < 256)
					palette_clr[palette_sz] = src_clr;
				palette_sz++;
			}
		}
		if (palette_sz < 256)
			type = GFX_8BIT_PALETTE;

		if (gs->use_alpha)
		{
			if (!gs->alpha_premultiplied)
				type |= GFX_ALPHA_STRAIGHT;
			else
				type |= GFX_ALPHA_PREMULTIPLIED;
		}

		fputc(type, fh);
		fwrite(&gs->w, sizeof(unsigned int), 1, fh);
		fwrite(&gs->h, sizeof(unsigned int), 1, fh);

		bool* used = new bool[gs->w * gs->h];
		if (!used)
		{
			return false;
		}

		memset(used, 0, sizeof(bool) * gs->w * gs->h);

		//minimale rechteckgröße in quadratpixel, die eine kompression sinnvoll macht
		int min_rect_size = 10;
		if (type & GFX_24BIT)
			min_rect_size = 4;

		//nach blöcken (rect) gleicher farbe suchen, die zusammengefasst werden können
		SmallRect* rcs = NULL;
		int szRcs = 0;
		int idx = 0;
		unsigned int compare_clr;

		rcs = new SmallRect[gs->w * gs->h / min_rect_size];

		int cur_x, cur_y;
		bool xend, yend;

		for (int y = 0; y < gs->h - 1; y++)
		{
			for (int x = 0; x < gs->w - 1; x++)
			{
				if (used[y * gs->w + x])
					continue;

				if (gs->use_alpha)
					compare_clr = gs->bgra[(y * gs->w + x)];
				else
					compare_clr = gs->bgra[(y * gs->w + x)] & 0x00FFFFFF;

				cur_x = x + 1;
				cur_y = y + 1;
				xend = false;
				yend = false;
				while (!xend || !yend)
				{
					if (!xend)
					{
						for (int y2 = y; y2 < cur_y; y2++)
						{
							idx = (y2 * gs->w + cur_x);

							if (used[idx])
							{
								xend = true;
								break;
							}

							if (gs->use_alpha)
								src_clr = gs->bgra[idx];
							else
								src_clr = gs->bgra[idx] & 0x00FFFFFF;

							if (compare_clr != src_clr)
							{
								xend = true;
								break;
							}
						}
						if (!xend)
							cur_x++;
						if (cur_x >= gs->w)
							xend = true;
					}
					if (!yend)
					{
						for (int x2 = x; x2 < cur_x; x2++)
						{
							idx = (cur_y * gs->w + x2);

							if (used[idx])
							{
								xend = true;
								break;
							}

							if (gs->use_alpha)
								src_clr = gs->bgra[idx];
							else
								src_clr = gs->bgra[idx] & 0x00FFFFFF;

							if (compare_clr != src_clr)
							{
								yend = true;
								break;
							}
						}
						if (!yend)
							cur_y++;
						if (cur_y >= gs->h)
							yend = true;
					}
				}
				if ((cur_x - x) * (cur_y - y) >= min_rect_size)
				{
					//validate: falls überlappungen es unretabel machen
				/*	int counter = 0;
					for (unsigned int y2 = y; y2 < cur_y; y2++)
					{
						for (unsigned int x2 = x; x2 < cur_x; x2++)
						{
							if (!used[y2 * gs->w + x2])
							{
								counter++;
								if (counter >= min_rect_size)
									break;
							}
						}
					}*/

					//	if (counter >= min_rect_size)
					//	{
					rcs[szRcs].Left = (short)x;
					rcs[szRcs].Top = (short)y;
					rcs[szRcs].Right = (short)cur_x;
					rcs[szRcs].Bottom = (short)cur_y;
					szRcs++;
					for (int y2 = y; y2 < cur_y; y2++)
					{
						memset(&used[y2 * gs->w + x], true, sizeof(bool) * (cur_x - x));
					}
					//	}
				}
			}
		}

		if (type & GFX_24BIT)
		{
			//encodestep 1
			fwrite(&szRcs, sizeof(unsigned int), 1, fh);

			for (int n = 0; n < szRcs; n++)
			{
				if (gs->use_alpha)
					fwrite(&p8[(rcs[n].Left + rcs[n].Top * gs->w) * 4], 4, 1, fh);
				else
					fwrite(&p8[(rcs[n].Left + rcs[n].Top * gs->w) * 4], 3, 1, fh);

				fwrite(&rcs[n], sizeof(SmallRect), 1, fh);
			}
			//encodestep 2
			int parts = 0;
			for (int i = 0; i < gs->w * gs->h; i++)
			{
				if (!used[i])
					parts++;
			}
			int* ids = new int[parts];
			parts = 0;
			for (int i = 0; i < gs->w * gs->h; i++)
			{
				if (!used[i])
				{
					ids[parts] = i;
					parts++;
				}
			}
			unsigned int clr[256];
			int clr_id = 0;
			int clr_counter[256];
			memset(clr_counter, 0, sizeof(int) * 256);

			for (int y = 0; y < parts; y++)
			{
				if (gs->use_alpha)
					src_clr = gs->bgra[ids[y]];
				else
					src_clr = gs->bgra[ids[y]] & 0x00FFFFFF;

				bool there = false;
				for (int n = 0; n < clr_id; n++)
				{
					if (src_clr == clr[n])
					{
						clr_counter[n]++;
						there = true;
						break;
					}
				}
				if (!there)
				{
					clr[clr_id] = src_clr;
					clr_counter[clr_id]++;
					clr_id++;
				}
				if (y > 0 && (((y + 1) % 256) == 0 || y == parts - 1))
				{
					unsigned int part_size = (y % 256);

					unsigned int palette_clr_count = 0;
					for (int n = 0; n < clr_id; n++)
					{
						if (clr_counter[n] > 1)
							palette_clr_count++;
					}
					fputc(palette_clr_count, fh);
					for (int n = 0; n < clr_id; n++)
					{
						if (clr_counter[n] > 1)
						{
							if (gs->use_alpha)
								fwrite(&clr[n], 4, 1, fh);
							else
								fwrite(&clr[n], 3, 1, fh);

							fputc(clr_counter[n], fh);

							int count = 0;
							for (unsigned int v = 0; v <= part_size; v++)
							{
								if (gs->use_alpha)
									compare_clr = gs->bgra[ids[y - part_size + v]];
								else
									compare_clr = gs->bgra[ids[y - part_size + v]] & 0x00FFFFFF;

								if (clr[n] == compare_clr)
								{
									used[ids[y - part_size + v]] = true;
									fputc(v, fh);
									count++;
								}
							}
						}
					}
					clr_id = 0;
					memset(clr_counter, 0, sizeof(int) * 256);
				}
			}
			delete[] ids;

			//bitmap (dont use rle compression
	//		unsigned char count = 0;
	//		bool raw = false;
			for (int n = 0; n < gs->w * gs->h; n++)
			{
				if (!used[n])
				{
					/*	if (count < 1)
						{
							//count rle pixels
							for (count = 1; count < 128; count++)
							{
								if (n + count >= gs->w * gs->h -1 || gs->bgra[n] != gs->bgra[n + count])
								{
									break;
								}
							}

							if (count > 1)
							{
								unsigned char wcount = count | 0x80;
								fwrite(&wcount, 1, 1, fh);
								raw = false;
							}
							else
							{
								//count raw pixels
								for (count = 2; count < 128; count++)
								{
									if (n + count >= gs->w * gs->h -1 || gs->bgra[n + count - 1] == gs->bgra[n + count])
									{
										count--;
										break;
									}
								}

								fwrite(&count, 1, 1, fh);
								raw = true;
							}


							if (gs->use_alpha)
								fwrite(&gs->bgra[n], 4, 1, fh);
							else
								fwrite(&gs->bgra[n], 3, 1, fh);
						}
						else if (raw)
						{*/
					if (gs->use_alpha)
						fwrite(&gs->bgra[n], 4, 1, fh);
					else
						fwrite(&gs->bgra[n], 3, 1, fh);
					//	}
					//	count--;

				}
			}
		}
		else if (type & GFX_8BIT_PALETTE)
		{
			//write palette
			fputc(palette_sz, fh);
			for (int n = 0; n < palette_sz; n++)
			{
				if (gs->use_alpha)
					fwrite(&palette_clr[n], 4, 1, fh);
				else
					fwrite(&palette_clr[n], 3, 1, fh);
			}

			//encodestep 1
			fwrite(&szRcs, sizeof(unsigned int), 1, fh);

			for (int n = 0; n < szRcs; n++)
			{
				if (gs->use_alpha)
					src_clr = gs->bgra[(rcs[n].Left + rcs[n].Top * gs->w)];
				else
					src_clr = gs->bgra[(rcs[n].Left + rcs[n].Top * gs->w)] & 0x00FFFFFF;

				for (int i = 0; i < palette_sz; i++)
				{
					if (src_clr == palette_clr[i])
					{
						fputc(i, fh);
						break;
					}
				}

				fwrite(&rcs[n], sizeof(SmallRect), 1, fh);
			}
			//bitmap
			for (int n = 0; n < gs->w * gs->h; n++)
			{
				if (!used[n])
				{
					if (gs->use_alpha)
						src_clr = gs->bgra[n];
					else
						src_clr = gs->bgra[n] & 0x00FFFFFF;

					for (int i = 0; i < palette_sz; i++)
					{
						if (src_clr == palette_clr[i])
						{
							fputc(i, fh);
							break;
						}
					}
				}
			}
		}
		else
		{
			result = false;
		}

		SAFE_DELETE_ARRAY(rcs);

		fclose(fh);
		return result;
	}
	return false;
}

bool GFXEngine::SaveBmp(GFXSurface *gs, string name)
{
	if (_HasBGRABuffer(gs)) {
		FILE* fh;
		if (fopen_s(&fh, name.c_str(), "wb") != 0)
		{
			return false;
		}
		fputs("BM", fh);

		unsigned int clr[256];
		for (int n = 0; n < 256; n++)
			clr[n] = 0;
		int pClr = 0;
		int colorDepht = 16;
		unsigned char* p8 = (unsigned char*)gs->bgra;
		for (int y = 0; y < gs->h; y++)
		{
			for (int x = 0; x < gs->w; x++)
			{
				//mit bitmaske prüfen ob mit 16bit (565) farbverlust entsteht
				if ((p8[(y * gs->w + x) * 4 + 2] & 0x07) || (p8[(y * gs->w + x) * 4 + 1] & 0x03) || (p8[(y * gs->w + x) * 4] & 0x07))
					colorDepht = 24;
				if (pClr > 256)
					continue;
				bool there = false;
				for (int n = 0; n < pClr; n++)
				{
					if (RGB(p8[(y * gs->w + x) * 4 + 2],
						p8[(y * gs->w + x) * 4 + 1],
						p8[(y * gs->w + x) * 4]) == clr[n])
					{
						there = true;
						break;
					}
				}
				if (!there)
				{
					if (pClr > 255)
					{
						pClr++;
						break;
					}
					clr[pClr] = RGB(p8[(y * gs->w + x) * 4 + 2],
						p8[(y * gs->w + x) * 4 + 1],
						p8[(y * gs->w + x) * 4]);
					pClr++;
				}
			}
		}
		if (pClr <= 256)
		{
			colorDepht = 8;
			pClr = 256;
		}
		else
			pClr = 0;

		int terminator = 4 - ((gs->w * colorDepht / 8) % 4);
		if (terminator == 4)
			terminator = 0;
		int bitmapdatasize = (colorDepht / 8 * gs->w + terminator) * gs->h;
		int offset = 13 * 4 + 2 + 4 * pClr;
		if (colorDepht == 16)
			offset += 16;
		int filesize = bitmapdatasize + offset;

		//filesize
		fputc(filesize, fh);
		fputc(filesize >> 8, fh);
		fputc(filesize >> 16, fh);
		fputc(filesize >> 24, fh);
		//reserved
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		//offset
		fputc(offset, fh);
		fputc(offset >> 8, fh);
		fputc(offset >> 16, fh);
		fputc(offset >> 24, fh);
		//headersize
		fputc(0x28, fh);
		fputc(0 >> 8, fh);
		fputc(0 >> 16, fh);
		fputc(0 >> 24, fh);
		//width
		fputc(gs->w, fh);
		fputc(gs->w >> 8, fh);
		fputc(gs->w >> 16, fh);
		fputc(gs->w >> 24, fh);
		//height
		fputc(gs->h, fh);
		fputc(gs->h >> 8, fh);
		fputc(gs->h >> 16, fh);
		fputc(gs->h >> 24, fh);
		//planes
		fputc(1, fh);
		fputc(0, fh);
		//colordepht
		fputc(colorDepht, fh);
		fputc(colorDepht >> 8, fh);
		//compression
		if (colorDepht == 16)
			fputc(3, fh);
		else
			fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		//bitmapdatasize
		fputc(bitmapdatasize, fh);
		fputc(bitmapdatasize >> 8, fh);
		fputc(bitmapdatasize >> 16, fh);
		fputc(bitmapdatasize >> 24, fh);
		//h-resolution
		fputc(0x12, fh);
		fputc(0x0B, fh);
		fputc(0, fh);
		fputc(0, fh);
		//v-resolution
		fputc(0x12, fh);
		fputc(0x0B, fh);
		fputc(0, fh);
		fputc(0, fh);
		//colors (palette-size)
		fputc(pClr, fh);
		fputc(pClr >> 8, fh);
		fputc(pClr >> 16, fh);
		fputc(pClr >> 24, fh);
		//important colors
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);

		//palette
		for (int n = 0; n < pClr; n++)
		{
			fputc(GetBValue(clr[n]), fh);
			fputc(GetGValue(clr[n]), fh);
			fputc(GetRValue(clr[n]), fh);
			fputc(0, fh);
		}

		if (colorDepht == 16)
		{
			fputc(0, fh);
			fputc(0xF8, fh);
			fputc(0, fh);
			fputc(0, fh);

			fputc(0xE0, fh);
			fputc(0x07, fh);
			fputc(0, fh);
			fputc(0, fh);

			fputc(0x1F, fh);
			fputc(0, fh);
			fputc(0, fh);
			fputc(0, fh);

			fputc(0, fh);
			fputc(0, fh);
			fputc(0, fh);
			fputc(0, fh);
		}

		switch (colorDepht)
		{
		case 8:
		{
			for (int y = gs->h - 1; y >= 0; y--)
			{
				for (int x = 0; x < gs->w; x++)
				{
					for (int i = 0; i < pClr; i++)
					{
						if (clr[i] == RGB(p8[(y * gs->w + x) * 4 + 2],
							p8[(y * gs->w + x) * 4 + 1],
							p8[(y * gs->w + x) * 4]))
						{
							fputc(i, fh);
							break;
						}
					}
				}
				//terminator
				for (int n = 0; n < terminator; n++)
					fputc(0, fh);
			}
		}
		break;
		case 16:
		{
			for (int y = gs->h - 1; y >= 0; y--)
			{
				for (int x = 0; x < gs->w; x++)
				{
					unsigned short clr = RGB_565(
						p8[(y * gs->w + x) * 4 + 2],
						p8[(y * gs->w + x) * 4 + 1],
						p8[(y * gs->w + x) * 4]);
					fputc(clr, fh);
					fputc(clr >> 8, fh);
				}
				//terminator
				for (int n = 0; n < terminator; n++)
					fputc(0, fh);
			}
		}
		break;
		case 24:
		{
			for (int y = gs->h - 1; y >= 0; y--)
			{
				for (int x = 0; x < gs->w; x++)
				{
					fputc(p8[(y * gs->w + x) * 4], fh);
					fputc(p8[(y * gs->w + x) * 4 + 1], fh);
					fputc(p8[(y * gs->w + x) * 4 + 2], fh);
				}
				//terminator
				for (int n = 0; n < terminator; n++)
					fputc(0, fh);
			}
		}
		break;
		default:
		{
			return false;
		}
		}
		fclose(fh);
		return true;
	}
	return false;
}
bool GFXEngine::SaveAlphaChannelBmp(GFXSurface *gs, string name)
{
	if (_HasBGRABuffer(gs)) {
		FILE* fh;
		if (fopen_s(&fh, name.c_str(), "wb") != 0)
		{
			return false;
		}
		fputs("BM", fh);

		int pClr = 256;

		int terminator = 4 - (gs->w % 4);
		if (terminator == 4)
			terminator = 0;
		int bitmapdatasize = (gs->w + terminator) * gs->h;
		int offset = 13 * 4 + 2 + 4 * pClr;
		int filesize = bitmapdatasize + offset;

		//filesize
		fputc(filesize, fh);
		fputc(filesize >> 8, fh);
		fputc(filesize >> 16, fh);
		fputc(filesize >> 24, fh);
		//reserved
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		//offset
		fputc(offset, fh);
		fputc(offset >> 8, fh);
		fputc(offset >> 16, fh);
		fputc(offset >> 24, fh);
		//headersize
		fputc(0x28, fh);
		fputc(0 >> 8, fh);
		fputc(0 >> 16, fh);
		fputc(0 >> 24, fh);
		//width
		fputc(gs->w, fh);
		fputc(gs->w >> 8, fh);
		fputc(gs->w >> 16, fh);
		fputc(gs->w >> 24, fh);
		//height
		fputc(gs->h, fh);
		fputc(gs->h >> 8, fh);
		fputc(gs->h >> 16, fh);
		fputc(gs->h >> 24, fh);
		//planes
		fputc(1, fh);
		fputc(0, fh);
		//colordepht
		fputc(8, fh);
		fputc(0, fh);
		//compression
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		//bitmapdatasize
		fputc(bitmapdatasize, fh);
		fputc(bitmapdatasize >> 8, fh);
		fputc(bitmapdatasize >> 16, fh);
		fputc(bitmapdatasize >> 24, fh);
		//h-resolution
		fputc(0x12, fh);
		fputc(0x0B, fh);
		fputc(0, fh);
		fputc(0, fh);
		//v-resolution
		fputc(0x12, fh);
		fputc(0x0B, fh);
		fputc(0, fh);
		fputc(0, fh);
		//colors (palette-size)
		fputc(pClr, fh);
		fputc(pClr >> 8, fh);
		fputc(pClr >> 16, fh);
		fputc(pClr >> 24, fh);
		//important colors
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);
		fputc(0, fh);

		//palette
		for (int n = 0; n < 256; n++)
		{
			fputc(n, fh);
			fputc(n, fh);
			fputc(n, fh);
			fputc(0, fh);
		}

		unsigned char* p8 = (unsigned char*)gs->bgra;
		for (int y = gs->h - 1; y >= 0; y--)
		{
			for (int x = 0; x < gs->w; x++)
				fputc(p8[(y * gs->w + x) * 4 + 3], fh);
			//terminator
			for (int n = 0; n < terminator; n++)
				fputc(0, fh);
		}

		fclose(fh);
		return true;
	}
	return false;
}

void GFXEngine::FreeRAM(GFXSurface* gs) {
	if (gs && gs->bgra) {
		delete[] gs->bgra;
		gs->bgra = NULL;
	}
}

bool GFXEngine::_HasBGRABuffer(GFXSurface* gs) {

	if (!gs || !gs->bgra)
		return false;

	return true;
}
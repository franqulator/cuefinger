//Version 5.0 - 2023-11-24

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

#ifndef _GFX2D_H_
#define _GFX2D_H_

#ifdef __linux_wsl__
	#include "../include/SDL.h"
	#include "../include/SDL_ttf.h"
	#include "translator.h"

#define MAXINT32 0x7FFFFFFF
#elif __linux__ 
		#ifdef __ANDROID__
			#include <../SDL/include/SDL.h>
			#include <../SDL_ttf/SDL_ttf.h>
		#else
			#include <SDL2/SDL.h>
			#include <SDL2/SDL_ttf.h>
		#endif
		#include "translator.h"

	#define MAXINT32 0x7FFFFFFF

#elif _WIN32
	#include "../include/SDL.h"
	#include "../include/SDL_ttf.h"
	#include <windows.h>
	
	#ifdef _WIN64
		#pragma comment(lib,"../lib/x64/SDL2.lib")
		#pragma comment(lib,"../lib/x64/SDL2main.lib")
		#pragma comment(lib,"../lib/x64/SDL2_ttf.lib")
	#else
		#pragma comment(lib,"../lib/x86/SDL2.lib")
		#pragma comment(lib,"../lib/x86/SDL2main.lib")
		#pragma comment(lib,"../lib/x86/SDL2_ttf.lib")
	#endif
#endif

#include <stdio.h>
#include <string>
#include <time.h>
#include "misc.h"
#include "vector2d.h"
#include <deque>
#include <map>
#include <unordered_map>

using namespace std;

//#define DEBUG_INFO

const int INIT_JOB_STACK_SIZE = 512;

const int MAX_TEXTURE_WIDTH = 8192;
const int MAX_TEXTURE_HEIGHT = 8192;

const int FILTER_ACCURACY = 2;		// genauigkeit der filter als anzahl der kommastellen:
									// filter werden nur bei veränderung neu berechnet, d.h. es fallen ggf. neuberechnungen weg
									// die genauigkeit der berechnung nimmt jedoch ab

const int COLLISION_TANGENT_AREA = 8; //Bereich (Rechteck) um den Kollisionpunkt, der zur Berechnung der Tangente genutzt wird in Pixel, z.B. 3 ergibt ein Rechteck 17x17px

#define GFX_WINDOW				0x01
#define GFX_MAXIMIZED			0x02
#define GFX_FULLSCREEN			0x04
#define GFX_FULLSCREEN_DESKTOP	0x08
#define GFX_HARDWARE			0x10
#define GFX_SOFTWARE			0x20
#define GFX_HIDECURSOR			0x40
#define GFX_VSYNC			0x80

#define GFX_NONE			0x00
#define GFX_HFLIP			0x01
#define GFX_VFLIP			0x02
#define GFX_ADDITIV			0x04
#define GFX_USE_SPRITEBATCH		0x08
#define GFX_DESTINATION_ALPHA_MASK 0x10 //nur für drawonsurface

#define MAX_LAYERS				1

#define GFX_LAYER_BACK				0
#define GFX_LAYER_SPLIT_LEFT_BACK	1
#define GFX_LAYER_SPLIT_LEFT_FRONT	2
#define GFX_LAYER_SPLIT_RIGHT_BACK	3
#define GFX_LAYER_SPLIT_RIGHT_FRONT	4
#define GFX_LAYER_FRONT				5

#define GFX_VISIBLE			1.0f
#define GFX_HIDDEN			0.0f

#define GFX_RECTANGLE 		0x01
#define GFX_ELLIPSE			0x02

#define GFX_LEFT			0x00
#define GFX_CENTER			0x01
#define GFX_RIGHT			0x02
#define GFX_JUSTIFIED 		0x04
#define GFX_AUTOBREAK 		0x08

#define WHITE				0xffffffff
#define BLACK				0x00

#define OFF					0

#define BGR2RGB(b,g,r) 	(COLORREF)((b << 16) | (g << 8) | r)
#define RGB2BGR(rgb) 	(((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16) )

#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#define SAFE_DELETE_ARRAY(a) if( (a) != NULL ) delete[] (a); (a) = NULL;
#define SAFE_RELEASE(a) if( (a) != NULL ) a->Release(); (a) = NULL;

class GFXFilter
{
public:
	unsigned int shift_color;
	float shift_intensity;
	float brightness;
	float contrast;
	float gamma;
	float saturation;
	bool invert;

	GFXFilter() {
		this->shift_color = 0;
		this->shift_intensity = 0.0f;
		this->brightness = 1.0f;
		this->contrast = 1.0f;
		this->gamma = 1.0f;
		this->saturation = 0.0f;
		this->invert = false;
	}
};

class GFXFontProperties
{
public:
	float shadow_distance;
	float shadow_direction;
	unsigned int shadow_color;
	float shadow_opacity;
	unsigned int outline_color;
	float outline_opacity;
	float outline_strength;
	GFXFontProperties() {
		this->shadow_distance = 0.0f;
		this->shadow_direction = 0.0f;
		this->shadow_color = 0;
		this->shadow_opacity = 0.0f;
		this->outline_color = 0;
		this->outline_opacity = 0.0f;
		this->outline_strength = 0.0f;
	}
};

class GFXSurface;
class GFXFont;
class GFXSpriteBatch;


class GFXJob
{
public:
	GFXSurface* gs;
	float x;
	float y;
	Rect rc;

	//surface filter
	GFXFilter apply_filter;

	float opacity;
	float rotation;
	Vector2D rotationOffset;
	Vector2D stretch;
	int flags;

	unsigned int color;
	int shape;

	GFXFont* font;
	string text;
	int alignment;
	Vector2D max_size;
	GFXFontProperties properties;

	deque<GFXSpriteBatch*>* spritebatch; // zum sortieren auf gleiche grafiken und rendereffekten (dadurch können ggf. berechnungen der rendereffekte eingespart werden)

	int zorder;

	GFXJob() {
		init();
		this->x = 0.0f;
		this->y = 0.0f;
		this->opacity = 0.0;
		this->rotation = 0.0f;
		this->flags = 0;
		this->color = 0;
		this->alignment = 0;
		this->zorder = 0;
	}
	inline void init() {
		this->gs = NULL;
		this->font = NULL;
		this->shape = 0;
		this->spritebatch = NULL;
	}
};

class GFXSpriteBatch {
public:
	float x;
	float y;
	Rect rc;
	float opacity;
	float rotation;
	Vector2D rotationOffset;
	Vector2D stretch;
	GFXSpriteBatch(GFXJob *job) {
		this->x = job->x;
		this->y = job->y;
		this->rc = job->rc;
		this->opacity = job->opacity;
		this->rotation = job->rotation;
		this->rotationOffset = job->rotationOffset;
		this->stretch = job->stretch;
	}
};

struct cmp {
	bool operator() (GFXJob* a, GFXJob* b) const {
		// (brightness für vergleich ignorieren da kein softwarerendering notwendig) <- aktuell nope
		if ((a->flags & GFX_ADDITIV) != (b->flags & GFX_ADDITIV)) {
			return (int)(a->flags & GFX_ADDITIV) < (int)(b->flags & GFX_ADDITIV);
		}
		if (a->apply_filter.brightness != b->apply_filter.brightness) {
			return a->apply_filter.brightness < b->apply_filter.brightness;
		}
		if (a->apply_filter.shift_intensity != b->apply_filter.shift_intensity) {
			return a->apply_filter.shift_intensity < b->apply_filter.shift_intensity;
		}
		if (a->apply_filter.shift_color != b->apply_filter.shift_color) {
			return a->apply_filter.shift_color < b->apply_filter.shift_color;
		}
		if (a->apply_filter.saturation != b->apply_filter.saturation) {
			return a->apply_filter.saturation < b->apply_filter.saturation;
		}
		if (a->apply_filter.contrast != b->apply_filter.contrast) {
			return a->apply_filter.contrast < b->apply_filter.contrast;
		}
		if (a->apply_filter.gamma != b->apply_filter.gamma) {
			return a->apply_filter.gamma < b->apply_filter.gamma;
		}
		if (a->apply_filter.invert != b->apply_filter.invert) {
			return (int)a->apply_filter.invert < (int)b->apply_filter.invert;
		}
		return a->zorder < b->zorder;
	}
};

class GFXSurface
{
public:
	SDL_Texture **bitmap; // bitmap in GPU
	int x_surfaces;
	int y_surfaces;
	unsigned int *bgra; //bitmap im RAM
	int w; //width
	int h; //height
	bool use_alpha;
	bool alpha_premultiplied;
	bool will_delete;
	int will_draw;
	bool *collision_array;
	multimap<int, Point*>* borderPixel;

	//aktuell angewandte filter
	GFXFilter filter;

	//filtereinstellung für nächsten drawvorgang
	GFXFilter apply_filter;

	set<GFXJob*, cmp> jobs[MAX_LAYERS]; // liste der aktuellen draw-jobs, die dieses surface benutzen

	GFXSurface() {
		this->bitmap = NULL;
		this->x_surfaces = 0;
		this->y_surfaces = 0;
		this->bgra = NULL;
		this->w = 0;
		this->h = 0;
		this->use_alpha = false;
		this->alpha_premultiplied = false;
		this->will_delete = false;
		this->will_draw = 0;
		this->collision_array = NULL;
		this->borderPixel = NULL;
	}
};

class GFXFont
{
public:
	TTF_Font *ttf;
	unsigned int color;
	GFXFontProperties properties;

	bool will_delete;
	int will_draw;

	GFXFont()
	{
		this->ttf = NULL;
		this->color = 0;
		this->will_delete = false;
		this->will_draw = 0;
	}
};

class GFXTextRenderCache {
public:
	SDL_Texture* tx;
	bool used;
	GFXTextRenderCache(SDL_Texture *tx) {
		this->used = true;
		this->tx = tx;
	}
	~GFXTextRenderCache() {
		SDL_DestroyTexture(tx);
	}
};

class GFXEngine {
private:
	SDL_Window* window;
	SDL_Renderer *renderer;
	int renderer_width, renderer_height;

	int updateBgraSize;
	unsigned char *p_update_bgra; // buffer zum berechnen von filtern; wird dynamisch vergößert, falls notwendig

	deque<GFXJob*> jobsList[MAX_LAYERS];
	deque<GFXJob*>::iterator jobsIterator[MAX_LAYERS];

	set<GFXSurface*> spriteBatchRequests[MAX_LAYERS];
	int zorder[MAX_LAYERS];

	map<string, GFXTextRenderCache*> textRenderCache;

public:

//	Vector2D* debugScroll;

	//Grundfunktionen
	GFXEngine(string title, int x, int y, int w, int h, int flags, SDL_Window **r_window=NULL);
	~GFXEngine();
	bool Resize(int width = 0, int height = 0);
	bool Update();
	void AbortUpdate();

	//GFXSurface-Handlig
	GFXSurface* ScreenCapture(unsigned int x=0, unsigned int y=0, unsigned int w=0, unsigned int h=0);
	GFXSurface* CreateSurface(unsigned int w,unsigned int h);
	GFXSurface* CopySurface(GFXSurface *gs, unsigned int w=0, unsigned int h=0);
	void FreeRAM(GFXSurface* gs);
	bool 		DeleteSurface(GFXSurface* gs);

	//Zeichenfunktionen
	bool BeginDrawOnSurface(GFXSurface *gsurf_dest); //only to draw on a surface
	bool EndDrawOnSurface(GFXSurface *gsurf_dest);

	bool Draw(GFXSurface* gs,float x,float y,Rect* rc_src=NULL, 
				int flags=0, float opacity=1.0f, float rotation=0.0f, Vector2D *rotationOffset=NULL, Vector2D *stretch=NULL, int layer=0);
	bool Draw(GFXSurface* gs, float x, float y, Rect* rc_src,
		int flags, float opacity, float rotation, Vector2D* rotationOffset, float scale, int layer = 0);

	bool DrawOnSurface(GFXSurface *gs_dest, GFXSurface *gs_src, float x, float y, Rect* rc_src=NULL,
				int flags=0, float opacity=1.0f, float rotation=0.0f, Vector2D *rotationOffset=NULL, Vector2D *stretch=NULL,
				bool keep_src_surface=false); //keep_src_surface is ignored in SDL

	bool DrawShape(int shape, unsigned int color, float x, float y, float w, float h,
				int flags=0, float opacity = 1.0f, float rotation=0.0f, Vector2D *rotationOffset=NULL, int layer=0);

	bool DrawShapeOnSurface(GFXSurface *gs_dest, int shape, unsigned int color, float x, float y, float w, float h,
				int flags=0, float opacity = 1.0f, float rotation=0.0f, Vector2D *rotationOffset=NULL);

	void FreeDrawOnSurfaceSource(GFXSurface *gs_src); // unused in SDL

	//TODO
	//Filter
	void SetFilter_ColorShift(GFXSurface* gs, float intensity, unsigned int color = 0);
	void SetFilter_Brightness(GFXSurface* gs, float brightness);
	void SetFilter_Gamma(GFXSurface* gs, float gamma);
	void SetFilter_Contrast(GFXSurface* gs, float contrast);
	void SetFilter_Saturation(GFXSurface* gs, float saturation);
	void SetFilter_Invert(GFXSurface* gs, bool invert);
	bool ApplyFilter(GFXSurface* gs);
	//drawcolor elipse zeichnen

	//Alphachannelfunktionen
	bool LoadAlphaChannelBmp(GFXSurface* gsurf, string file);
	bool SaveAlphaChannelBmp(GFXSurface* gsurf, string file);
	bool EnableAlphaChannel(GFXSurface* gsurf, bool alpha);
	bool PremultiplyAlpha(GFXSurface* gsurf);

	//Lade- /Speicherfuntionen
	GFXSurface* LoadGfx(string file);
	GFXSurface* LoadGfxFromBuffer(unsigned char* data, size_t size);
	GFXSurface* LoadTga(string file);
	bool SaveGfx(GFXSurface* gs, string file);
	GFXSurface* LoadBmp(string file);
	bool SaveBmp(GFXSurface* gs, string file);

	//Collision
	bool CreateCollisionData(GFXSurface* gs, bool borderCheck = true);
	void DeleteCollisionData(GFXSurface* gs);

	//Punkt in Rechteck
	bool Collision(float pt_x, float pt_y, float r_x, float r_y, Rect* rc, float rotation = 0.0f, Vector2D* rotationOffset = NULL);
	// Punkt in Grafik
	bool Collision(float pt_x, float pt_y, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc = NULL, int flags = 0,
		float gs_rotation = 0.0f, Vector2D* rotationOffset = NULL, Vector2D* stretch = NULL);
	//Grafik in Grafik
	bool Collision(GFXSurface* gs1, float x1, float y1, Rect* rc1, int flags1, float rotation1, Vector2D* rotationOffset1, Vector2D* stretch1,
		GFXSurface* gs2, float x2, float y2, Rect* rc2, int flags2, float rotation2, Vector2D* rotationOffset2, Vector2D* stretch2, Vector2D* pt_col = NULL);
	// Punkt in Grafik
	bool Collision(float pt_x, float pt_y, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags,
		float gs_rotation, Vector2D* rotationOffset, float scale);
	//Grafik in Grafik
	bool Collision(GFXSurface* gs1, float x1, float y1, Rect* rc1, int flags1, float rotation1, Vector2D* rotationOffset1, float scale1,
		GFXSurface* gs2, float x2, float y2, Rect* rc2, int flags2, float rotation2, Vector2D* rotationOffset2, float scale2, Vector2D* pt_col = NULL);
	// Kreis in Kreis
	bool Collision(const float x1, const float y1, const float radius1,
		const float x2, const float y2, const float radius2, Vector2D* colPt = NULL);
	// Rechteck in Rechteck
	bool Collision(float x1, float y1, Rect* rc1, float rotation1, Vector2D* rotationOffset1,
		float x2, float y2, Rect* rc2, float rotation2, Vector2D* rotationOffset2, Vector2D* colPt = NULL); 
	Vector2D GetTangentAt(float pt_x, float pt_y, float direction, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, float rotation, Vector2D* rotationOffset, Vector2D* stretch, Vector2D* borderColPt = NULL);
	Vector2D GetTangentAt(float pt_x, float pt_y, float direction, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, float rotation, Vector2D* rotationOffset, float scale, Vector2D* borderColPt = NULL);

	//Font
	GFXFont* CreateFontFromFile(string ttf_path, int _height, bool bold=false, bool italic=false, bool underline=false);
	bool DeleteFont(GFXFont*);
	void SetColor(GFXFont* font, unsigned int color);
	void SetShadow(GFXFont* font, unsigned int color, float opacity, float distance = 1, float direction = M_PIF * 0.75f);
	void SetOutline(GFXFont* font, float strength, unsigned int color=BLACK, float opacity = 1.0);
	Vector2D GetTextBlockSize(GFXFont *font, string text, int alignment=GFX_LEFT, float max_width=0);
	void Write(GFXFont *font, float x, float y, const string &text, int alignment=GFX_LEFT, Vector2D *max_size=NULL,
					int flags=0, float opacity=1.0, float rotation=0, Vector2D * rotationOffset = NULL, Vector2D *stretch=NULL, int layer=0);

	void debugDrawLine(Vector2D pos, Vector2D v, unsigned int color, float strength = 3, Vector2D offset = Vector2D(0, 0));

private:
	//main
	bool _Draw(GFXSurface* gs_dest, GFXSurface* gs_src, int shape, unsigned int color, float pos_x, float pos_y, Rect* _rc = NULL,
		float opacity = 1.0, float rotation = 0, Vector2D* rotationOffset = NULL, Vector2D* stretch = NULL, int flags = 0, GFXFilter* apply_filter = NULL,
		int xoffset = 0, int yoffset = 0);
	void _Write(GFXSurface* gs, GFXFont* _font, unsigned int _color, float _x, float _y, const string &_text, int _alignment = GFX_LEFT,
		Vector2D* _max_size = NULL, float _opacity = 1.0, float _rotation = 0, Vector2D* _rotationOffset = NULL, Vector2D* _stretch = NULL, int _flags = 0);
	void _DrawAll();
	int _CreateSpriteBatches(int layer); // returns number of created batches
	int _CreateSpriteBatch(GFXSurface* gs, int layer); // returns number of sprites in that batch
	void _AddJob(GFXFont* font, float x, float y, const string &text, int alignment, Vector2D* max_size,
		int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer);
	bool _AddJob(GFXSurface* gs, int shape, unsigned int color, float x, float y, Rect* rc,
		int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer);
	bool _HasBGRABuffer(GFXSurface* gs);

	//fileio
	bool _ReadAlphaChannelBmp(GFXSurface* gs, FILE* fh);
	GFXSurface* _ReadTga(FILE* fh);
	GFXSurface* _ReadBmp(FILE* fh);

	//collision
	bool _CollisionBorderPointExists(GFXSurface* gs, int x, int y);
	Vector2D* _convertToScreen(Vector2D* v, float screenX, float screenY, Rect* rc = NULL, int flag = 0,
		float rotation = 0, Vector2D* rotationOffset = NULL, float scaleX = 1.0, float scaleY = 1.0);
	Vector2D* _convertFromScreen(Vector2D* v, float objX, float objY, Rect* rc = NULL, int flag = 0,
		float rotation = 0, Vector2D* rotationOffset = NULL, float scaleX = 1.0, float scaleY = 1.0);

	//font
	string _write_get_next_line(GFXFont *fnt,const string &txt, size_t *txt_ptr, float width, bool autobreak);

	// filter
	bool _ApplyFilter(GFXSurface* gs_src, GFXFilter* apply_filter, bool render_manual = false);
	bool _CopyColorInfo(GFXSurface* gsurf, bool premultiplyAlpha = false, bool applyFilter = false);
	void _RenderFilter(GFXSurface* gs, unsigned char* p_update_bgra, Rect* rc = NULL);
	bool _compareFilter(float filter1, float filter2); // vergleicht unter berücksichtigung der FILTER_ACCURACY
	bool _equal(GFXFilter* filter1, GFXFilter* filter2, bool excludeHWFilter=false); // nutzt _compareFilter als vergleicher
};

#endif

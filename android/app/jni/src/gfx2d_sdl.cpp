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

#include "gfx2d_sdl.h"

GFXJob::GFXJob() {
	init();
}
void GFXJob::init() {
	this->gs = NULL;
	this->font = NULL;
	this->shape = 0;
	this->spritebatch = NULL;
}

void GFXEngine::debugDrawLine(Vector2D pos, Vector2D v, unsigned int color, float strength, Vector2D offset) {
	Vector2D rotationCenter(-v.length() / 2, strength / 2);
	DrawShape(0, color,
		pos.getX() - strength / 2 + offset.getX(),
		pos.getY() + offset.getY(), v.length(), strength,
		0, 1, v.getAngle(), &rotationCenter, GFX_LAYER_FRONT);

	/*	GFX_DrawShape(0, RGB(255,0,0),
			pos.getX() + offset.getX(),
			pos.getY() + offset.getY(), 3, 3,
			0, 1, 0, NULL, GFX_LAYER_FRONT);

		pos.add(&v);
		GFX_DrawShape(0, RGB(255, 0, 0),
			pos.getX() + offset.getX(),
			pos.getY() + offset.getY(), 3, 3,
			0, 1, 0, NULL, GFX_LAYER_FRONT);*/
}

void GFXEngine::_AddJob(GFXFont* font, float x, float y, string text, short alignment, Vector2D* max_size,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer)
{
	if (!font || layer >= MAX_LAYERS)
		return;

	font->will_draw++;

	if (!text.length())
	{
		return;
	}

	GFXJob* job = NULL;

	if (jobsList[layer].empty() || jobsIterator[layer] == jobsList[layer].end()) {
		job = new GFXJob;
		jobsList[layer].push_back(job);
		jobsIterator[layer] = jobsList[layer].end();
	}
	else {
		job = (*jobsIterator[layer]);
		jobsIterator[layer]++;
		job->init();
	}

	job->font = font;
	job->color = font->color;
	job->text = text;
	job->alignment = alignment;

	if (max_size)
		job->max_size = *max_size;
	else
		job->max_size = Vector2D(0.0f, 0.0f);

	memcpy(&job->properties, &font->properties, sizeof(GFXFontProperties));

	job->opacity = opacity;
	job->rotation = rotation;
	if (rotationOffset)
		job->rotationOffset = *rotationOffset;
	else
		job->rotationOffset = Vector2D(0.0f, 0.0f);
	if (stretch)
		job->stretch = *stretch;
	else
		job->stretch = Vector2D(NAN, NAN);

	job->flags = flags;

	job->x = x;
	job->y = y;

	if (layer == GFX_LAYER_SPLIT_LEFT_BACK || layer == GFX_LAYER_SPLIT_LEFT_FRONT)
	{
		job->x -= (float)renderer_width / 4.0f;
	}
	else if (layer == GFX_LAYER_SPLIT_RIGHT_BACK || layer == GFX_LAYER_SPLIT_RIGHT_FRONT)
	{
		job->x += (float)renderer_width / 4.0f;
	}

	job->zorder = this->zorder[layer];
	this->zorder[layer]++;
}

bool GFXEngine::_AddJob(GFXSurface* gs, int shape, unsigned int color, float x, float y, Rect* rc,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer)
{
	if (layer >= MAX_LAYERS)
		return false;

	if (gs)
		gs->will_draw++;

	GFXJob* job = NULL;

	if (jobsList[layer].empty() || jobsIterator[layer] == jobsList[layer].end()) {
		job = new GFXJob;
		jobsList[layer].push_back(job);
		jobsIterator[layer] = jobsList[layer].end();
	}
	else {
		job = (*jobsIterator[layer]);
		jobsIterator[layer]++;
		job->init();
	}

	job->gs = gs;
	job->color = color;
	job->shape = shape;
	job->x = x;
	job->y = y;
	if (rc)
	{
		job->rc.left = rc->left;
		job->rc.top = rc->top;
		job->rc.right = rc->right;
		job->rc.bottom = rc->bottom;
	}
	else if (gs)
	{
		job->rc.left = 0;
		job->rc.top = 0;
		job->rc.right = gs->w;
		job->rc.bottom = gs->h;
	}
	else
	{
		job->rc.left = 0;
		job->rc.top = 0;
		job->rc.right = renderer_width;
		job->rc.bottom = renderer_height;
	}

	if (layer == GFX_LAYER_SPLIT_LEFT_BACK || layer == GFX_LAYER_SPLIT_LEFT_FRONT)
	{
		job->x -= (float)renderer_width / 4.0f;
	}
	else if (layer == GFX_LAYER_SPLIT_RIGHT_BACK || layer == GFX_LAYER_SPLIT_RIGHT_FRONT)
	{
		job->x += (float)renderer_width / 4.0f;
	}

	job->opacity = opacity;
	job->rotation = rotation;
	if (rotationOffset)
		job->rotationOffset = *rotationOffset;
	else
		job->rotationOffset = Vector2D(0.0f, 0.0f);
	if (stretch)
		job->stretch = *stretch;
	else
		job->stretch = Vector2D(NAN, NAN);

	job->flags = flags;

	if (gs) {
		memcpy(&job->apply_filter, &gs->apply_filter, sizeof(GFXFilter));
	}

	Vector2D center(0, 0);
	if (stretch) {
		center.set(stretch->getX() / 2.0f, stretch->getY() / 2.0f);
	}
	else {
		center.set((float)job->rc.getWidth() / 2.0f, (float)job->rc.getHeight() / 2.0f);
	}
	float radius = center.length();

	bool remove_job = job->rc.getWidth() < 1 || job->rc.getHeight() < 1
		|| center.getX() + radius < 1.0f || center.getX() - radius > (float)renderer_width
		|| center.getY() + radius < 1.0f || center.getY() - radius > (float)renderer_height;

	if (remove_job) {
		jobsIterator[layer]--;
		return false;
	}

	if ((flags & GFX_USE_SPRITEBATCH) && gs) { // in liste zur überprüfung auf mögliches spritebatching hinzufügen
		gs->jobs[layer].insert(job);
		this->spriteBatchRequests[layer].insert(gs);
	}

	job->zorder = this->zorder[layer];
	this->zorder[layer]++;

	return true;
}

GFXEngine::GFXEngine(string title, int x, int y, int width, int height, int flags, SDL_Window** r_window)
{
	this->window = NULL;
	this->renderer = NULL;
	this->renderer_width = 0;
	this->renderer_height = 0;
	this->offline_render_texture = NULL;
	this->updateBgraSize = 0;
	this->p_update_bgra = NULL;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
		SDL_Init(SDL_INIT_VIDEO);

	TTF_Init();

	if (flags & GFX_HIDECURSOR)
		SDL_ShowCursor(SDL_DISABLE);

	Uint32 window_flags = /*SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL|*/ SDL_WINDOW_RESIZABLE /*| SDL_WINDOW_ALLOW_HIGHDPI*/;
	if ((flags & GFX_FULLSCREEN))
	{
		window_flags |= SDL_WINDOW_FULLSCREEN;
	}
	else if (flags & GFX_FULLSCREEN_DESKTOP)
	{
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	else if ((flags & GFX_MAXIMIZED))
	{
		window_flags |= SDL_WINDOW_MAXIMIZED;
	}

	// Create an application window with the following settings:
	window = SDL_CreateWindow(title.c_str(), x, y, width, height, window_flags);
	if (window == NULL) {
		throw invalid_argument("Error in GFXEngine: SDL_CreateWindow -> " + string(SDL_GetError()));
	}

	if (r_window)
	{
		*r_window = window;
	}

	Uint32 render_flags = SDL_RENDERER_ACCELERATED;
	if (flags & GFX_SOFTWARE)
	{
		render_flags = SDL_RENDERER_SOFTWARE;
	}

	if (flags & GFX_VSYNC)
	{
		render_flags |= SDL_RENDERER_PRESENTVSYNC;
	}
    render_flags |= SDL_RENDERER_TARGETTEXTURE;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);
	renderer = SDL_CreateRenderer(window, -1, render_flags);
	if (!renderer) {
		throw invalid_argument("Error in GFXEngine: SDL_CreateRenderer -> " + string(SDL_GetError()));
	}

	/*	window_surface = SDL_GetWindowSurface(window);
		if(!window_surface) {
			throw invalid_argument("Error in GFXEngine: SDL_GetWindowSurface -> " + string(SDL_GetError()));
		}
	*/
	SDL_RenderSetLogicalSize(renderer, width, height);

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	renderer_width = width;
	renderer_height = height;

	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	//offline render texture for prerendering text
	offline_render_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING,
		renderer_width * xscale, renderer_height * yscale);
	if (!offline_render_texture) {
		throw invalid_argument("Error in GFXEngine: SDL_CreateTexture -> " + string(SDL_GetError()));
	}
	SDL_SetTextureBlendMode(offline_render_texture, SDL_BLENDMODE_BLEND);

	for (int i = 0; i < MAX_LAYERS; i++) {
		for (int n = 0; n < INIT_JOB_STACK_SIZE; n++) {
			jobsList[i].push_back(new GFXJob);
		}
		jobsIterator[i] = jobsList[i].begin();
	}
}

bool GFXEngine::Resize(unsigned int width, unsigned int height) {

	if (width == 0 && height == 0) {
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		width = w;
		height = h;
	}

	SDL_RenderSetLogicalSize(renderer, width, height);

	renderer_width = width;
	renderer_height = height;

	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	if (offline_render_texture) {
		SDL_DestroyTexture(offline_render_texture);
		offline_render_texture = NULL;
	}

	//offline render texture for prerendering text
	offline_render_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING,
		renderer_width * xscale, renderer_height * yscale);
	if (!offline_render_texture) {
		return false;
	}
	SDL_SetTextureBlendMode(offline_render_texture, SDL_BLENDMODE_BLEND);

	return true;
}

GFXEngine::~GFXEngine() {
	AbortUpdate();

	SAFE_DELETE_ARRAY(p_update_bgra);

	for (int i = 0; i < MAX_LAYERS; i++) {
		while (!jobsList[i].empty()) {
			SAFE_DELETE(jobsList[i].back());
			jobsList[i].pop_back();
		}
	}

	if (offline_render_texture) {
		SDL_DestroyTexture(offline_render_texture);
		offline_render_texture = NULL;
	}

	if (renderer) {
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	/*	if(window_surface) {
			//don't use free since it's handled by the window
			window_surface = NULL;
		}*/

	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	TTF_Quit();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool GFXEngine::Update() {
	// falls innerhalb eines BeginDraw - EndDraw-Blocks aufgerufen wird, renderTarget vorrübergehend auf Ausgabe setzen 
	SDL_Texture* prev_renderTarget = SDL_GetRenderTarget(renderer);
	if (prev_renderTarget)
		SDL_SetRenderTarget(renderer, NULL);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	_DrawAll();

	SDL_RenderPresent(renderer);

	if (prev_renderTarget)
		SDL_SetRenderTarget(renderer, prev_renderTarget);

	AbortUpdate();// zum reset der werte

	return true;
}

string GFXEngine::_write_get_next_line(GFXFont* fnt, string txt, size_t* txt_ptr, float width, bool autobreak) {
	int estimate_width;
	int estimate_height;

	string txt_line;

	size_t fi = txt.find_first_of("\n", *txt_ptr);
	if (fi == string::npos)
	{
		txt_line = txt.substr(*txt_ptr, txt.length() - *txt_ptr);
	}
	else
	{
		txt_line = txt.substr(*txt_ptr, fi - *txt_ptr + 1);
	}

	if (autobreak) // automatischer zeilenumbruch
	{
		TTF_SizeUTF8(fnt->ttf, txt_line.c_str(), &estimate_width, &estimate_height);
		if (width > 0 && estimate_width > width)
		{
			for (int txt_len = 1; txt_len < txt.length() - *txt_ptr; txt_len++)
			{
				txt_line = txt.substr(*txt_ptr, txt_len);

				TTF_SizeUTF8(fnt->ttf, txt_line.c_str(), &estimate_width, &estimate_height);

				if (estimate_width > width)
				{
					fi = txt_line.find_last_of(" -\t");
					if (fi != string::npos)
					{
						txt_len = fi + 1;
					}
					else
						txt_len--;

					if (txt_len < 1)
						txt_len = 1;

					txt_line = txt_line.substr(0, txt_len);
					break;
				}
			}
		}
	}
	*txt_ptr += txt_line.length();

	return txt_line;
}


int GFXEngine::_CreateSpriteBatch(GFXSurface* gs, int layer)
{
	if (!renderer || !gs || renderer_width <= 0 || renderer_height <= 0)
		return 0;

	if (gs->jobs[layer].size() < 2)
		return 0;

	int sprites = 0;
	GFXJob* mainJob = NULL;

	for (set<GFXJob*>::iterator it = gs->jobs[layer].begin(); it != gs->jobs[layer].end(); ++it) {

		GFXJob* subJob = *it;
		if (!mainJob) {
			mainJob = subJob;
		}
		bool createNewBatch = (subJob == mainJob) || !_equal(&subJob->apply_filter, &mainJob->apply_filter, true);

		if (createNewBatch) { // ziel für spritebatch neu setzen
			mainJob = subJob;
		}

		GFXSpriteBatch* batch = new GFXSpriteBatch();
		batch->x = subJob->x;
		batch->y = subJob->y;
		batch->rc = subJob->rc;
		batch->opacity = subJob->opacity;
		batch->rotation = subJob->rotation;
		batch->rotationOffset = subJob->rotationOffset;
		batch->stretch = subJob->stretch;

		if (!mainJob->spritebatch) {
			mainJob->spritebatch = new deque<GFXSpriteBatch*>();
		}

		mainJob->spritebatch->push_back(batch);

		if (subJob != mainJob) {
			//ggf. in z-order nach vorne holen (vertauschen)
			if (subJob->zorder > mainJob->zorder) {
				subJob->spritebatch = mainJob->spritebatch;
				subJob->flags = mainJob->flags;
				memcpy(&subJob->apply_filter, &mainJob->apply_filter, sizeof(GFXFilter));
				mainJob->init();
				mainJob = subJob;
			}
			else {
				// subjob deaktivieren, da er vom batch in mainJob gezeichnet wird
				subJob->init();
			}
			gs->will_draw--;
		}
		sprites++;
	}

	return sprites;
}

int GFXEngine::_CreateSpriteBatches(int layer) {

	int result = 0;

	for (set<GFXSurface*>::iterator it = this->spriteBatchRequests[layer].begin(); it != this->spriteBatchRequests[layer].end(); ++it) {
		if ((*it)->jobs[layer].size() > 1) { // mehr als ein drawvorgang
			if (_CreateSpriteBatch((*it), layer) != 0) {
				result++;
			}
		}
	}

	return result;
}

void GFXEngine::_DrawAll()
{
	for (int layer = 0; layer < MAX_LAYERS; layer++)
	{
		if (jobsList[layer].empty())
			continue;

		_CreateSpriteBatches(layer);

		if (layer == GFX_LAYER_SPLIT_LEFT_BACK || layer == GFX_LAYER_SPLIT_LEFT_FRONT)
		{
			SDL_Rect rcClip = { 0, 0, renderer_width / 2, renderer_height };
			SDL_RenderSetClipRect(renderer, &rcClip);
		}
		else if (layer == GFX_LAYER_SPLIT_RIGHT_BACK || layer == GFX_LAYER_SPLIT_RIGHT_FRONT)
		{
			SDL_Rect rcClip = { renderer_width / 2, 0, renderer_width / 2, renderer_height };
			SDL_RenderSetClipRect(renderer, &rcClip);
		}
		else
		{
			SDL_RenderSetClipRect(renderer, NULL);
		}

		for (deque<GFXJob*>::iterator it = jobsList[layer].begin(); it != jobsIterator[layer]; ++it)
		{
			if ((*it)->font) //Schreibe Text
			{
				if ((*it)->properties.shadow_distance)
				{
					float tx = cos((*it)->properties.shadow_direction) * (*it)->properties.shadow_distance;
					float ty = sin((*it)->properties.shadow_direction) * (*it)->properties.shadow_distance;

					_Write(NULL, (*it)->font, (*it)->properties.shadow_color,
						(*it)->x + tx, (*it)->y + ty,
						(*it)->text, (*it)->alignment, &(*it)->max_size,
						(*it)->opacity * (*it)->properties.shadow_opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
						(*it)->flags);
				}
				if ((*it)->properties.outline_strength)
				{
					//left
					_Write(NULL, (*it)->font, (*it)->properties.outline_color,
						(*it)->x - (*it)->properties.outline_strength, (*it)->y,
						(*it)->text, (*it)->alignment, &(*it)->max_size,
						(*it)->opacity * (*it)->properties.outline_opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
						(*it)->flags);

					//top
					_Write(NULL, (*it)->font, (*it)->properties.outline_color,
						(*it)->x, (*it)->y - (*it)->properties.outline_strength,
						(*it)->text, (*it)->alignment, &(*it)->max_size,
						(*it)->opacity * (*it)->properties.outline_opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
						(*it)->flags);

					//right
					_Write(NULL, (*it)->font, (*it)->properties.outline_color,
						(*it)->x + (*it)->properties.outline_strength, (*it)->y,
						(*it)->text, (*it)->alignment, &(*it)->max_size,
						(*it)->opacity * (*it)->properties.outline_opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
						(*it)->flags);

					//bottom
					_Write(NULL, (*it)->font, (*it)->properties.outline_color,
						(*it)->x, (*it)->y + (*it)->properties.outline_strength,
						(*it)->text, (*it)->alignment, &(*it)->max_size,
						(*it)->opacity * (*it)->properties.outline_opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
						(*it)->flags);
				}
				_Write(NULL, (*it)->font, (*it)->color,
					(*it)->x, (*it)->y,
					(*it)->text, (*it)->alignment, &(*it)->max_size,
					(*it)->opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
					(*it)->flags);
			}
			else if ((*it)->spritebatch) //batch
			{
				//reset filter that can be drawn by hardware
			//	(*it)->apply_filter.brightness = 1.0f;
				for (deque<GFXSpriteBatch*>::iterator it2 = (*it)->spritebatch->begin(); it2 != (*it)->spritebatch->end(); ++it2) {

					_Draw(NULL, (*it)->gs, 0, 0, (*it2)->x, (*it2)->y, &(*it2)->rc,
						(*it2)->opacity, (*it2)->rotation, &(*it2)->rotationOffset, &(*it2)->stretch,
						(*it)->flags, &(*it)->apply_filter);
				}
			}
			else if ((*it)->gs || (*it)->shape)
			{
				_Draw(NULL, (*it)->gs, (*it)->shape, (*it)->color, (*it)->x, (*it)->y, &(*it)->rc,
					(*it)->opacity, (*it)->rotation, &(*it)->rotationOffset, &(*it)->stretch,
					(*it)->flags, &(*it)->apply_filter);
			}
		}
	}
}

void GFXEngine::AbortUpdate()
{
	for (int layer = 0; layer < MAX_LAYERS; layer++)
	{
		for (deque<GFXJob*>::iterator it = jobsList[layer].begin(); it != jobsIterator[layer]; ++it)
		{
			if ((*it)->spritebatch) {
				for (deque<GFXSpriteBatch*>::iterator it2 = (*it)->spritebatch->begin(); it2 != (*it)->spritebatch->end(); ++it2) {
					SAFE_DELETE(*it2);
				}
				(*it)->spritebatch->clear();
				SAFE_DELETE((*it)->spritebatch);
			}
			if ((*it)->gs) {
				(*it)->gs->jobs[layer].clear();
				(*it)->gs->will_draw--;
				if ((*it)->gs->will_delete && (*it)->gs->will_draw < 1)
				{
					DeleteSurface((*it)->gs);
					(*it)->gs = NULL;
				}
			}
			if ((*it)->font) {
				(*it)->font->will_draw = 0;
				if ((*it)->font->will_delete && (*it)->font->will_draw < 1)
				{
					DeleteFont((*it)->font);
					(*it)->font = NULL;
				}
			}
		}
		jobsIterator[layer] = jobsList[layer].begin();
		spriteBatchRequests[layer].clear();
		zorder[layer] = 0;
	}
}

GFXSurface* GFXEngine::CreateSurface(unsigned int w, unsigned int h)
{
	GFXSurface* gs = new GFXSurface;
	if (!gs)
	{
		return NULL;
	}
	gs->bgra = new unsigned int[w * h];
	if (!gs->bgra)
	{
		DeleteSurface(gs);
		return NULL;
	}

	memset(gs->bgra, 255, sizeof(unsigned int) * w * h);

	gs->w = w;
	gs->h = h;

	gs->x_surfaces = ceil((double)w / MAX_TEXTURE_WIDTH);
	gs->y_surfaces = ceil((double)h / MAX_TEXTURE_HEIGHT);

	if (!gs->x_surfaces || !gs->y_surfaces)
	{
		DeleteSurface(gs);
		return NULL;
	}

	gs->bitmap = new SDL_Texture * [gs->x_surfaces * gs->y_surfaces];
	if (!gs->bitmap)
	{
		DeleteSurface(gs);
		return NULL;
	}

	for (int y = 0; y < gs->y_surfaces; y++)
	{
		for (int x = 0; x < gs->x_surfaces; x++)
		{
			int bmWidth = 0;
			int bmHeight = 0;

			gs->bitmap[x + y * gs->x_surfaces] = NULL;

			if (x == gs->x_surfaces - 1)
				bmWidth = gs->w - x * MAX_TEXTURE_WIDTH;
			else
				bmWidth = MAX_TEXTURE_WIDTH;

			if (y == gs->y_surfaces - 1)
				bmHeight = gs->h - y * MAX_TEXTURE_HEIGHT;
			else
				bmHeight = MAX_TEXTURE_HEIGHT;

			gs->bitmap[x + y * gs->x_surfaces] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
				/*SDL_TEXTUREACCESS_TARGET | SDL_TEXTUREACCESS_STREAMING |*/ SDL_TEXTUREACCESS_STATIC, bmWidth, bmHeight);
			if (!gs->bitmap[x + y * gs->x_surfaces])
			{
				DeleteSurface(gs);
				return NULL;
			}

			SDL_SetTextureBlendMode(gs->bitmap[x + y * gs->x_surfaces], SDL_BLENDMODE_NONE);
		}
	}

	return gs;
}

GFXSurface* GFXEngine::CopySurface(GFXSurface* gs_src, unsigned int w, unsigned int h)
{
	if (!gs_src)
		return NULL;

	int new_w = gs_src->w;
	int new_h = gs_src->h;

	bool create_collision_array = (bool)gs_src->collision_array;

	if (w)
		new_w = w;

	if (h)
		new_h = h;

	GFXSurface* gs = NULL;
	gs = CreateSurface(new_w, new_h);
	if (!gs)
		return NULL;

	gs->alpha_premultiplied = gs_src->alpha_premultiplied;

	EnableAlphaChannel(gs, gs_src->use_alpha);

	if (new_w == gs_src->w && new_h == gs_src->h)//copy
	{
		if (_HasBGRABuffer(gs_src)) {
			memcpy(gs->bgra, gs_src->bgra, sizeof(unsigned int) * gs->w * gs->h);

			if (!_CopyColorInfo(gs))
			{
				DeleteSurface(gs);
				return NULL;
			}

			if (create_collision_array)
			{
				CreateCollisionData(gs);
			}
		}
	}
	else//resize
	{
		BeginDrawOnSurface(gs);
		Vector2D stretch((float)new_w, (float)new_h);
		DrawOnSurface(gs, gs_src, 0, 0, NULL, 0, 1.0, 0, NULL, &stretch);
		EndDrawOnSurface(gs);

		//create collision array happens in enddraw
	}

	return gs;
}

bool GFXEngine::_CopyColorInfo(GFXSurface* gs, bool premultiplyAlpha, bool applyFilter)
//copies pixels from ram to sdl-surface
{
	if (!gs)
		return false;

	if (applyFilter)
	{
		if (this->updateBgraSize < gs->w * gs->h * 4) {
			SAFE_DELETE_ARRAY(p_update_bgra);
			this->updateBgraSize = gs->w * gs->h * 4;
			p_update_bgra = new unsigned char[this->updateBgraSize];
			if (!p_update_bgra)
				return false;
		}
	}

	unsigned char* p_byte_bgra = (unsigned char*)gs->bgra;

	/*	if (premultiplyAlpha && gs->use_alpha && !gs->alpha_premultiplied)
		{
			for (unsigned int y = 0; y < gs->h; y++)
			{
				for (unsigned int x = 0; x < gs->w; x++)
				{
					int idx = (y*gs->w + x) * 4;
					float mul = (float)p_byte_bgra[idx + 3] / 255;

					p_byte_bgra[idx] = (unsigned char)roundf((float)p_byte_bgra[idx] * mul);
					p_byte_bgra[idx + 1] = (unsigned char)roundf((float)p_byte_bgra[idx + 1] * mul);
					p_byte_bgra[idx + 2] = (unsigned char)roundf((float)p_byte_bgra[idx + 2] * mul);
				}
			}
			gs->alpha_premultiplied = true;
		}
		//sdl untertützt kein premultiplied alpha -> zu straight alpha konvertieren
		else */if (gs->use_alpha && gs->alpha_premultiplied)
		{
			for (unsigned int y = 0; y < gs->h; y++)
			{
				for (unsigned int x = 0; x < gs->w; x++)
				{
					int idx = (y * gs->w + x) * 4;
					float mul = 255.0 / (float)p_byte_bgra[idx + 3];

					p_byte_bgra[idx] = (unsigned char)roundf((float)p_byte_bgra[idx] * mul);
					p_byte_bgra[idx + 1] = (unsigned char)roundf((float)p_byte_bgra[idx + 1] * mul);
					p_byte_bgra[idx + 2] = (unsigned char)roundf((float)p_byte_bgra[idx + 2] * mul);
				}
			}
			gs->alpha_premultiplied = false;
		}

		if (applyFilter)
		{
			_RenderFilter(gs, p_update_bgra);
			p_byte_bgra = p_update_bgra;
		}

		for (int sx = 0; sx < gs->x_surfaces; sx++)
		{
			for (int sy = 0; sy < gs->y_surfaces; sy++)
			{
				//	Rect rcT = { 0,0,gs->w,gs->h };

				int maxwidth = MAX_TEXTURE_WIDTH;
				int maxheight = MAX_TEXTURE_HEIGHT;

				//randsurfaces können kleiner sein (rest)
				if (sx == gs->x_surfaces - 1)
					maxwidth = gs->w - sx * MAX_TEXTURE_WIDTH;
				if (sy == gs->y_surfaces - 1)
					maxheight = gs->h - sy * MAX_TEXTURE_HEIGHT;

				if (maxwidth < 1 || maxheight < 1)
					continue;

				SDL_Rect rcSDL;
				rcSDL.x = 0;
				rcSDL.y = 0;
				rcSDL.w = maxwidth;
				rcSDL.h = maxheight;
				int result = SDL_UpdateTexture(gs->bitmap[sx + sy * gs->x_surfaces], &rcSDL,
					&p_byte_bgra[4 * ((sy * MAX_TEXTURE_HEIGHT) * gs->w + sx * MAX_TEXTURE_WIDTH)],
					/*&p_byte_bgra[4 * ((sy * MAX_TEXTURE_HEIGHT + rcT.top) * gs->w + sx * MAX_TEXTURE_WIDTH + rcT.left)],*/ gs->w * 4);

				/*	void *pixels;
					int pitch = 0;
					int result = SDL_LockTexture(gs->bitmap[sx + sy * gs->x_surfaces], &rcSDL, (void**)&pixels, &pitch);
					if (result != 0) {
						string error(SDL_GetError());
						return false;
					}
					memcpy(pixels, &p_byte_bgra[4 * ((sy * MAX_TEXTURE_HEIGHT) * gs->w + sx * MAX_TEXTURE_WIDTH)], pitch * maxheight);
					SDL_UnlockTexture(gs->bitmap[sx + sy * gs->x_surfaces]);*/
			}
		}

		return true;
}

bool GFXEngine::DeleteSurface(GFXSurface* gs)
{
	if (gs)
	{
		if (gs->will_draw > 0)
		{
			gs->will_delete = true;
			return false;
		}
		if (gs->bitmap)
		{
			for (int n = 0; n < gs->x_surfaces * gs->y_surfaces; n++)
			{
				if (gs->bitmap[n])
				{
					SDL_DestroyTexture(gs->bitmap[n]);
					gs->bitmap[n] = NULL;
				}
			}
			SAFE_DELETE_ARRAY(gs->bitmap);
		}
		SAFE_DELETE_ARRAY(gs->bgra);

		DeleteCollisionData(gs);

		SAFE_DELETE(gs);
		return true;
	}
	return false;
}

bool GFXEngine::BeginDrawOnSurface(GFXSurface* gs_dest)
{
	if (!gs_dest)
		return false;

	for (int n = 0; n < gs_dest->x_surfaces * gs_dest->y_surfaces; n++)
	{
		SDL_SetRenderTarget(renderer, gs_dest->bitmap[n]);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
	}
	return true;
}

bool GFXEngine::EndDrawOnSurface(GFXSurface* gs_dest)
{
	if (!gs_dest)
		return false;

	int w = gs_dest->w;
	int h = gs_dest->h;

	if (w >= MAX_TEXTURE_WIDTH)
	{
		w = MAX_TEXTURE_WIDTH;
	}
	if (h >= MAX_TEXTURE_HEIGHT)
	{
		h = MAX_TEXTURE_HEIGHT;
	}

	unsigned int* buffer = new unsigned int[w * h];
	if (!buffer)
		return false;

	if (!_HasBGRABuffer(gs_dest)) {
		gs_dest->bgra = new unsigned int[gs_dest->w * gs_dest->h];
	}

	for (int sy = 0; sy < gs_dest->y_surfaces; sy++)
	{
		for (int sx = 0; sx < gs_dest->x_surfaces; sx++)
		{
			SDL_SetRenderTarget(renderer, gs_dest->bitmap[sx + sy * gs_dest->x_surfaces]);

			int bm_width = gs_dest->w - sx * MAX_TEXTURE_WIDTH;
			int bm_height = gs_dest->h - sy * MAX_TEXTURE_HEIGHT;

			if (bm_width > MAX_TEXTURE_WIDTH)
			{
				bm_width = MAX_TEXTURE_WIDTH;
			}
			if (bm_height > MAX_TEXTURE_HEIGHT)
			{
				bm_height = MAX_TEXTURE_HEIGHT;
			}

			SDL_Rect rc;
			rc.x = 0;
			rc.y = 0;
			rc.w = bm_width;
			rc.h = bm_height;
			if (SDL_RenderReadPixels(renderer, &rc, SDL_PIXELFORMAT_BGRA32, (void*)buffer, 4 * bm_width) != 0)
			{
				SAFE_DELETE_ARRAY(buffer);
				return false;
			}

			for (int y = 0; y < bm_height; y++)
			{
				memcpy(&gs_dest->bgra[(sy * MAX_TEXTURE_HEIGHT + y) * gs_dest->w + sx * MAX_TEXTURE_WIDTH], &buffer[y * bm_width], 4 * bm_width);
			}
		}
	}

	SAFE_DELETE_ARRAY(buffer);

	SDL_SetRenderTarget(renderer, NULL);

	if (gs_dest->collision_array)
	{
		CreateCollisionData(gs_dest);
	}

	return _CopyColorInfo(gs_dest);
}

bool GFXEngine::_Draw(GFXSurface* gs_dest, GFXSurface* gs_src, int shape, unsigned int color, float pos_x, float pos_y, Rect* _rc,
	float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int flags, GFXFilter* apply_filter,
	int xoffset, int yoffset)
{
	if (!renderer || !_rc)
		return false;

	int bm_x = ((int)pos_x - xoffset) / MAX_TEXTURE_WIDTH;
	int bm_y = ((int)pos_y - yoffset) / MAX_TEXTURE_HEIGHT;

	float tiled_pos_x = pos_x;
	float tiled_pos_y = pos_y;

	if (gs_dest)
	{
		tiled_pos_x -= bm_x * MAX_TEXTURE_WIDTH;
		tiled_pos_y -= bm_y * MAX_TEXTURE_HEIGHT;

		if (bm_x >= gs_dest->x_surfaces || bm_x < 0
			|| bm_y >= gs_dest->y_surfaces || bm_y < 0)
			return false;

		SDL_SetRenderTarget(renderer, gs_dest->bitmap[bm_x + bm_y * gs_dest->x_surfaces]);
	}

	Rect l_rc = { 0, 0, 0, 0 };
	if (_rc)
	{
		l_rc = *_rc;
	}
	else if (gs_src)
	{
		l_rc.right = gs_src->w;
		l_rc.bottom = gs_src->h;
	}

	float x_ratio = 1.0f;
	float y_ratio = 1.0f;

	if (stretch && !isnan(stretch->getX()) && !isnan(stretch->getY()) && l_rc.getWidth() > 0 && l_rc.getHeight() > 0)
	{
		x_ratio = stretch->getX() / (float)(_rc->getWidth());
		y_ratio = stretch->getY() / (float)(_rc->getHeight());
	}


	if (gs_src)//bitmap
	{
		if (_HasBGRABuffer(gs_src)) {
			_ApplyFilter(gs_src, apply_filter);
		}

		SDL_Rect rc;
		SDL_FRect destRC;
		int lastwidth = 0, lastheight = 0;
		for (int y = 0; y < gs_src->y_surfaces; y++)
		{
			lastwidth = 0;
			rc.y = l_rc.top - y * MAX_TEXTURE_HEIGHT;
			rc.h = -rc.y + l_rc.bottom - y * MAX_TEXTURE_HEIGHT;

			int maxheight = MAX_TEXTURE_HEIGHT;
			if (y == gs_src->y_surfaces - 1)
				maxheight = gs_src->h - y * MAX_TEXTURE_HEIGHT;

			if (rc.y < 0)
				rc.y = 0;
			if (rc.h > maxheight - rc.y)
				rc.h = maxheight - rc.y;

			if (rc.h <= 0)
				continue;

			destRC.y = tiled_pos_y + lastheight;
			destRC.h = rc.h * y_ratio;

			for (int x = 0; x < gs_src->x_surfaces; x++)
			{
				rc.x = l_rc.left - x * MAX_TEXTURE_WIDTH;
				rc.w = -rc.x + l_rc.right - x * MAX_TEXTURE_WIDTH;

				int maxwidth = MAX_TEXTURE_WIDTH;
				if (x == gs_src->x_surfaces - 1)
					maxwidth = gs_src->w - x * MAX_TEXTURE_WIDTH;

				if (rc.x < 0)
					rc.x = 0;
				if (rc.w > maxwidth - rc.x)
					rc.w = maxwidth - rc.x;

				if (rc.w <= 0)
					continue;

				if (flags & GFX_ADDITIV) {
					SDL_SetTextureBlendMode(gs_src->bitmap[x + y * gs_src->x_surfaces], SDL_BLENDMODE_ADD);
				}
				else if (opacity < 1.0f || gs_src->use_alpha) {
					SDL_SetTextureBlendMode(gs_src->bitmap[x + y * gs_src->x_surfaces], SDL_BLENDMODE_BLEND);
				}
				else {
					SDL_SetTextureBlendMode(gs_src->bitmap[x + y * gs_src->x_surfaces], SDL_BLENDMODE_NONE);
				}

				SDL_SetTextureAlphaMod(gs_src->bitmap[x + y * gs_src->x_surfaces], (Uint8)(opacity * 255.0f));

				destRC.x = tiled_pos_x + lastwidth;
				destRC.w = rc.w * x_ratio;

				SDL_FPoint center;
				center.x = (float)l_rc.getWidth() / 2.0f;
				center.y = (float)l_rc.getHeight() / 2.0f;
				center.x *= x_ratio;
				center.y *= y_ratio;

				if (rotationOffset)
				{
					center.x += rotationOffset->getX();
					center.y += rotationOffset->getY();
				}

				int flip = SDL_FLIP_NONE;
				if (flags & GFX_HFLIP) {
					flip = SDL_FLIP_HORIZONTAL;
				}
				if (flags & GFX_VFLIP) {
					flip |= SDL_FLIP_VERTICAL;
				}
				if (SDL_RenderCopyExF(renderer, gs_src->bitmap[x + y * gs_src->x_surfaces],
					&rc, &destRC, RAD2DEG(rotation), &center, (SDL_RendererFlip)flip) != 0)
				{
					return false;
				}

				//	SDL_SetRenderDrawColor(renderer, 255, 0, 0, (Uint8)50);
				//	SDL_RenderFillRectF(renderer, &destRC);

				lastwidth += destRC.w;
			}
			lastheight += destRC.h;
		}

		if (gs_dest)
		{
			float width = (float)l_rc.getWidth();
			float height = (float)l_rc.getHeight();

			width *= x_ratio;
			height *= y_ratio;

			float radius = sqrt(pow(width, 2.0f) + pow(height, 2.0f)) / 2.0f;

			float center_pos_x = pos_x + width / 2.0f;
			float center_pos_y = pos_y + height / 2.0f;

			if (rotationOffset)
			{
				center_pos_x += rotationOffset->getX();
				center_pos_y += rotationOffset->getY();
			}

			//grafik auf links folgende teiltexturen zeichnen weiterzeichnen
			if (xoffset >= 0 && !yoffset &&
				center_pos_x - radius + xoffset < 0)
			{
				_Draw(gs_dest, gs_src, 0, 0, pos_x, pos_y, &l_rc,
					opacity, rotation, rotationOffset, stretch, flags, NULL,
					xoffset + MAX_TEXTURE_WIDTH, yoffset);
			}
			//grafik auf rechts folgende teiltexturen zeichnen weiterzeichnen
			if (xoffset <= 0 && !yoffset &&
				center_pos_x + radius + xoffset > MAX_TEXTURE_WIDTH)
			{
				_Draw(gs_dest, gs_src, 0, 0, pos_x, pos_y, &l_rc,
					opacity, rotation, rotationOffset, stretch, flags, NULL,
					xoffset - MAX_TEXTURE_WIDTH, yoffset);
			}
			//grafik auf nach oben folgende weiterzeichnen
			if (yoffset >= 0 &&
				center_pos_y - radius + yoffset < 0)
			{
				_Draw(gs_dest, gs_src, 0, 0, pos_x, pos_y, &l_rc,
					opacity, rotation, rotationOffset, stretch, flags, NULL,
					xoffset, yoffset + MAX_TEXTURE_HEIGHT);
			}
			//grafik auf nach unten folgende weiterzeichnen
			if (yoffset <= 0 &&
				center_pos_y + radius + yoffset > MAX_TEXTURE_HEIGHT)
			{
				_Draw(gs_dest, gs_src, 0, 0, pos_x, pos_y, &l_rc,
					opacity, rotation, rotationOffset, stretch, flags, NULL,
					xoffset, yoffset - MAX_TEXTURE_HEIGHT);
			}
		}
	}
	else if (shape)//shape
	{
		SDL_SetRenderDrawColor(renderer, GetRValue(color), GetGValue(color), GetBValue(color), (Uint8)(opacity * 255.0f));

		if (shape == GFX_ELLIPSE)
		{
		}
		else //rect
		{
			SDL_FRect rc;
			rc.x = pos_x;
			rc.y = pos_y;
			rc.w = l_rc.right - l_rc.left;
			rc.h = l_rc.bottom - l_rc.top;

			SDL_RenderFillRectF(renderer, &rc);
		}
	}

	return true;
}
bool GFXEngine::DrawOnSurface(GFXSurface* gs_dest, GFXSurface* gs_src, float x, float y, Rect* rc_src,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, bool keep_src_surface) // keep_src_surface is ignored in SDL
{
	return _Draw(gs_dest, gs_src, 0, 0, x, y, rc_src, opacity, rotation, rotationOffset, stretch, flags, &gs_src->apply_filter);
}

void GFXEngine::FreeDrawOnSurfaceSource(GFXSurface* gs_src) {} // unused in SDL-Version

bool GFXEngine::Draw(GFXSurface* gs, float x, float y, Rect* rc,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, float scale, int layer) {

	if (!gs) {
		return false;
	}

	Vector2D stretch(gs->w * scale, gs->h * scale);
	if (rc) {
		stretch.set((float)rc->getWidth() * scale, (float)rc->getHeight() * scale);
	}
	return Draw(gs, x, y, rc, flags, opacity, rotation, rotationOffset, &stretch, layer);
}

bool GFXEngine::Draw(GFXSurface* gs, float x, float y, Rect* rc,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer)
{
	if (!gs)
	{
		return false;
	}
	return _AddJob(gs, 0, 0, x, y, rc, flags, opacity, rotation, rotationOffset, stretch, layer);
}

bool GFXEngine::DrawShapeOnSurface(GFXSurface* gs_dest, int shape, unsigned int color, float x, float y, float w, float h,
	int flags, float opacity, float rotation, Vector2D* rotationOffset)
{
	Rect rc_src;
	rc_src.left = 0;
	rc_src.top = 0;
	rc_src.right = w;
	rc_src.bottom = h;
	return _Draw(gs_dest, NULL, shape, color, x, y, &rc_src, opacity, rotation, rotationOffset, NULL, flags);
}

bool GFXEngine::DrawShape(int shape, unsigned int color, float x, float y, float w, float h,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, int layer)
{
	Rect rc = { 0,0,(int)w,(int)h };

	return _AddJob(NULL, shape, color, x, y, &rc, flags, opacity, rotation, rotationOffset, NULL, layer);
}

bool GFXEngine::EnableAlphaChannel(GFXSurface* gs, bool alpha)
{
	if (!gs)
		return false;

	bool result = true;

	gs->use_alpha = alpha;
	/*
		if(alpha)
		{
			for(int n=0; n < gs->x_surfaces * gs->y_surfaces; n++) {
				if(SDL_SetTextureBlendMode(gs->bitmap[n], SDL_BLENDMODE_BLEND) != 0) {
					result = false;
				}
			}
		}
		else
		{
			for(int n=0; n < gs->x_surfaces * gs->y_surfaces; n++)
				if(SDL_SetTextureBlendMode(gs->bitmap[n], SDL_BLENDMODE_NONE)  != 0) {
					result = false;
				}
		}*/
	return result;
}

GFXSurface* GFXEngine::ScreenCapture(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	int scaled_renderer_width = (int)((float)renderer_width * xscale);
	int scaled_renderer_height = (int)((float)renderer_height * yscale);

	int wndW = 0;
	int wndH = 0;
	SDL_GetWindowSize(window, &wndW, &wndH);

	int xoffset = (wndW - scaled_renderer_width) / 2;
	int yoffset = (wndH - scaled_renderer_height) / 2;

	int scaled_x = (int)((float)x * xscale);
	int scaled_y = (int)((float)y * yscale);
	int scaled_w = (int)((float)w * xscale);
	int scaled_h = (int)((float)h * yscale);

	if (scaled_x + scaled_w > scaled_renderer_width || scaled_y + scaled_h > scaled_renderer_height)
		return NULL;

	if (scaled_w < 0)
		scaled_w = scaled_renderer_width - scaled_x;
	if (scaled_h < 0)
		scaled_h = scaled_renderer_height - scaled_y;

	GFXSurface* gsurf = CreateSurface(scaled_w, scaled_h);
	if (!gsurf)
		return NULL;

	SDL_Rect rc;
	rc.x = xoffset;
	rc.y = yoffset;
	rc.w = scaled_w;
	rc.h = scaled_h;
	if (SDL_RenderReadPixels(renderer, &rc, SDL_PIXELFORMAT_BGRA32, (void*)gsurf->bgra, 4 * scaled_renderer_width) != 0)
	{
		SAFE_DELETE(gsurf);
		return NULL;
	}

	_CopyColorInfo(gsurf);
	return gsurf;
}


// ****   FONT   ****
// ******************
GFXFont* GFXEngine::CreateFontFromFile(string ttf_path, int _height, bool bold, bool italic, bool underline)
{
	GFXFont* fnt;
	fnt = new GFXFont;
	if (!fnt)
	{
		return fnt;
	}

	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	fnt->ttf = TTF_OpenFont(ttf_path.c_str(), _height * yscale);
	if (!fnt->ttf)
	{
		SAFE_DELETE(fnt);
		return NULL;
	}

	int style = TTF_STYLE_NORMAL;
	if (bold)
	{
		style |= TTF_STYLE_BOLD;
	}
	if (italic)
	{
		style |= TTF_STYLE_ITALIC;
	}
	if (underline)
	{
		style |= TTF_STYLE_UNDERLINE;
	}

	TTF_SetFontStyle(fnt->ttf, style);

	return fnt;
}

bool GFXEngine::DeleteFont(GFXFont* font)
{
	if (font)
	{
		if (font->will_draw > 0)
		{
			font->will_delete = true;
			return false;
		}
		else if (font->ttf)
		{
			TTF_CloseFont(font->ttf);
		}
		SAFE_DELETE(font);
	}
	return true;
}

//gs für zukünftige nutzung falls man auf surface render möchte
void GFXEngine::_Write(GFXSurface* gs, GFXFont* _font, unsigned int _color, float _x, float _y, string _text, int _alignment,
	Vector2D* _max_size, float _opacity, float _rotation, Vector2D* _rotationOffset, Vector2D* _stretch, int _flags)
{
	if (!_font)
		return;

	SDL_Color color = { GetRValue(_color),
					GetGValue(_color),
					GetBValue(_color),
					255 };

	bool done = false;
	size_t txt_ptr = 0;
	int line_nr = 0;
	string txt_line;

	float max_width = 0;
	if (_max_size)
		max_width = _max_size->getX();

	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	float line_height = (float)TTF_FontHeight(_font->ttf) / yscale;

	do
	{
		string txt_line = _write_get_next_line(_font, _text, &txt_ptr, max_width * xscale, (_alignment & GFX_AUTOBREAK));

		SDL_Surface* sf_text = TTF_RenderUTF8_Blended(_font->ttf, txt_line.c_str(), color);
		if (sf_text)
		{
			float sf_width = (float)sf_text->w / xscale;
			float sf_height = (float)sf_text->h / yscale;

			if (SDL_LockSurface(sf_text) == 0)
			{
				void* pixels;
				int pitch;
				SDL_Rect clip_rect = sf_text->clip_rect;
				clip_rect.w++;
				clip_rect.h++;

                SDL_Texture *tx_text = SDL_CreateTextureFromSurface(renderer, sf_text);
                if(tx_text)
                {
                    float x_ratio = 1.0f;
                    float y_ratio = 1.0f;

                    if (_stretch)
                    {
                        if (!isnan(_stretch->getX()) && !isnan(_stretch->getY()) && sf_width > 0.0f && sf_height > 0.0f)
                        {
                            x_ratio = _stretch->getX() / sf_width;
                            y_ratio = _stretch->getY() / sf_height;
                        }
                    }

                    SDL_FRect destRC;
                    destRC.x = _x;
                    destRC.y = _y + (float)line_nr * line_height;

                    destRC.w = sf_width * x_ratio;
                    destRC.h = sf_height * y_ratio;

                    if ((_alignment & GFX_CENTER))
                    {
                        destRC.x -= sf_width / 2.0f;
                        destRC.x += max_width / 2.0f;
                    }
                    else if ((_alignment & GFX_RIGHT))
                    {
                        destRC.x -= sf_width;
                        destRC.x += max_width;
                    }

                    if (_flags & GFX_ADDITIV)
                    {
                        SDL_SetTextureBlendMode(tx_text, SDL_BLENDMODE_ADD);
                    }
                    else if (_opacity < 1.0f)
                    {
                        SDL_SetTextureBlendMode(tx_text, SDL_BLENDMODE_BLEND);
                    }

                    SDL_SetTextureAlphaMod(tx_text, (Uint8)(_opacity * 255.0f));

                    SDL_FPoint center;
                    center.x = destRC.w / 2.0f;
                    center.y = destRC.h / 2.0f;

                    if (_rotationOffset)
                    {
                        center.x += _rotationOffset->getX();
                        center.y += _rotationOffset->getY();
                    }

                    int flip = SDL_FLIP_NONE;
                    if (_flags & GFX_HFLIP)
                    {
                        flip = SDL_FLIP_HORIZONTAL;
                    }
                    if (_flags & GFX_VFLIP)
                    {
                        flip |= SDL_FLIP_VERTICAL;
                    }

                    SDL_RenderCopyExF(renderer, tx_text, &sf_text->clip_rect, &destRC, RAD2DEG(_rotation), &center, (SDL_RendererFlip)flip);

                    SDL_DestroyTexture(tx_text);
                }
				SDL_UnlockSurface(sf_text);
			}
			SDL_FreeSurface(sf_text);
		}
		line_nr++;
	} while (txt_ptr < _text.length());
}

void GFXEngine::Write(GFXFont* font, float x, float y, string text, int alignment, Vector2D* max_size,
	int flags, float opacity, float rotation, Vector2D* rotationOffset, Vector2D* stretch, int layer)
{
	_AddJob(font, x, y, text, alignment, max_size, flags, opacity, rotation, rotationOffset, stretch, layer);
}

Vector2D GFXEngine::GetTextBlockSize(GFXFont* font, string text, int alignment, float max_width)
{
	Vector2D sz = { 0,0 };

	if (!font)
		return sz;

	size_t txt_ptr = 0;
	int w, h;

	float xscale, yscale;
	SDL_RenderGetScale(renderer, &xscale, &yscale);

	do
	{
		string txt_line = _write_get_next_line(font, text, &txt_ptr, max_width * xscale, (alignment & GFX_AUTOBREAK));
		TTF_SizeText(font->ttf, txt_line.c_str(), &w, &h);

		sz.setX(max(sz.getX(), (float)w));
		sz.addY((float)h);
	} while (txt_ptr < text.length());

	sz.multiplyX(1 / xscale);
	sz.multiplyY(1 / yscale);

	return sz;
}

void GFXEngine::SetColor(GFXFont* font, unsigned int _color)
{
	if (!font)
		return;

	font->color = _color;
}

void GFXEngine::SetShadow(GFXFont* font, unsigned int color, float opacity, float distance, float direction)
{
	if (!font)
		return;

	font->properties.shadow_color = color;
	font->properties.shadow_opacity = opacity;
	font->properties.shadow_distance = distance;
	font->properties.shadow_direction = direction;
}

void GFXEngine::SetOutline(GFXFont* font, float strength, unsigned int color, float opacity)
{
	if (!font)
		return;

	font->properties.outline_strength = strength;
	font->properties.outline_color = color;
	font->properties.outline_opacity = opacity;
}

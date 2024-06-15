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

bool GFXEngine::PremultiplyAlpha(GFXSurface *gs)
{
	return _CopyColorInfo(gs, true);
}
void GFXEngine::_RenderFilter(GFXSurface *gs, unsigned char *p_update_bgra, Rect *rc) //returns the rendered pixels but keeps the original bgra values of the surface
{
	if (!gs)
		return;

	unsigned int start_x=0, end_x=gs->w;
	unsigned int start_y=0, end_y=gs->h;

	if (rc)
	{
		start_x = rc->left;
		end_x = rc->right;
		start_y = rc->top;
		end_y = rc->bottom;
	}
	unsigned int pitch = end_x - start_x;

	//colorshift
	unsigned char scb = GetBValue(gs->filter.shift_color);
	unsigned char scg = GetGValue(gs->filter.shift_color);
	unsigned char scr = GetRValue(gs->filter.shift_color);
	unsigned char si = (unsigned char)(gs->filter.shift_intensity * 255.0f);

	float bt_res; // brightness
	float gw;
	short sat_res; //saturation
	float ct_res; //contrast
	unsigned char gammaValues[256];

	if (gs->filter.gamma != 1.0f)
	{
		float gv;
		for (int clr = 0; clr < 256; clr++)
		{
			gv = powf((float)clr / 255.0f, 1.0f / gs->filter.gamma) * 255.0f;
			gammaValues[clr] = (unsigned char)gv;
		}
	}

	int offset = (start_y * gs->w + start_x);
	unsigned char *p_byte_bgra = (unsigned char*)&gs->bgra[offset];
	unsigned int idx = 0;

	for (unsigned int y = 0; y < end_y - start_y; y++)
	{
		for (unsigned int x = 0; x < end_x - start_x; x++)
		{
			idx = (y * gs->w + x) * 4;
			
			p_byte_bgra = (unsigned char*)&gs->bgra[offset];

			//set alphachannel
			p_update_bgra[(y * pitch + x) * 4 + 3] = p_byte_bgra[idx + 3];

			if (gs->use_alpha && !p_byte_bgra[idx + 3])
			{
				p_update_bgra[(y * pitch + x) * 4] = p_byte_bgra[idx];
				p_update_bgra[(y * pitch + x) * 4 + 1] = p_byte_bgra[idx + 1];
				p_update_bgra[(y * pitch + x) * 4 + 2] = p_byte_bgra[idx + 2];
			}
			else
			{
				bool reset = true;

				//convert from premultiplied alpha to straight
				if (gs->use_alpha && gs->alpha_premultiplied && (gs->filter.invert || gs->filter.brightness != 1.0f || gs->filter.contrast != 1.0f ||
					gs->filter.gamma != 1.0f || gs->filter.saturation != 0.0f || gs->filter.shift_intensity != 0.0f))
				{
					float mul = 255.0f / (float)p_byte_bgra[idx + 3];
					p_update_bgra[(y * pitch + x) * 4] = (unsigned char)round((float)p_byte_bgra[idx] * mul);
					p_update_bgra[(y * pitch + x) * 4 + 1] = (unsigned char)round((float)p_byte_bgra[idx + 1] * mul);
					p_update_bgra[(y * pitch + x) * 4 + 2] = (unsigned char)round((float)p_byte_bgra[idx + 2] * mul);
					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
				}

				if (gs->filter.invert)
				{
					p_update_bgra[(y * pitch + x) * 4] = 255 - p_byte_bgra[idx];
					p_update_bgra[(y * pitch + x) * 4 + 1] = 255 - p_byte_bgra[idx + 1];
					p_update_bgra[(y * pitch + x) * 4 + 2] = 255 - p_byte_bgra[idx + 2];

					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
					reset = false;
				}
				if (gs->filter.shift_intensity != 0.0f)
				{
					p_update_bgra[(y * pitch + x) * 4] = (si * (scb - p_byte_bgra[idx]) >> 8) + p_byte_bgra[idx];
					p_update_bgra[(y * pitch + x) * 4 + 1] = (si * (scg - p_byte_bgra[idx + 1]) >> 8) + p_byte_bgra[idx + 1];
					p_update_bgra[(y * pitch + x) * 4 + 2] = (si * (scr - p_byte_bgra[idx + 2]) >> 8) + p_byte_bgra[idx + 2];

					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
					reset = false;
				}
				if (gs->filter.contrast != 1.0f)
				{
					ct_res = (float)(p_byte_bgra[idx] - 127) * gs->filter.contrast + 127;
					if (ct_res > 255)
						p_update_bgra[(y * pitch + x) * 4] = 255;
					else if (ct_res < 0)
						p_update_bgra[(y * pitch + x) * 4] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4] = ct_res;

					ct_res = (float)(p_byte_bgra[idx + 1] - 127) * gs->filter.contrast + 127;
					if (ct_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 1] = 255;
					else if (ct_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 1] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 1] = ct_res;

					ct_res = (float)(p_byte_bgra[idx + 2] - 127) * gs->filter.contrast + 127;
					if (ct_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 2] = 255;
					else if (ct_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 2] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 2] = ct_res;

				/*	ct_res = (float)(p_byte_bgra[idx + 3] - 127) * gs->filter.contrast + 127;
					if (ct_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 3] = 255;
					else if (ct_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 3] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 3] = ct_res;*/

					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
					reset = false;
				}
				if (gs->filter.brightness != 1.0f)
				{
					bt_res = (float)p_byte_bgra[idx] * gs->filter.brightness;
					if (bt_res > 255)
						p_update_bgra[(y * pitch + x) * 4] = 255;
					else if (bt_res < 0)
						p_update_bgra[(y * pitch + x) * 4] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4] = (unsigned char)bt_res;

					bt_res = (float)p_byte_bgra[idx + 1] * gs->filter.brightness;
					if (bt_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 1] = 255;
					else if (bt_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 1] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 1] = (unsigned char)bt_res;

					bt_res = (float)p_byte_bgra[idx + 2] * gs->filter.brightness;
					if (bt_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 2] = 255;
					else if (bt_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 2] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 2] = (unsigned char)bt_res;

				/*	bt_res = (float)p_byte_bgra[idx + 3] * gs->filter.brightness;
					if (bt_res > 255)
						p_update_bgra[(y * pitch + x) * 4 + 3] = 255;
					else if (bt_res < 0)
						p_update_bgra[(y * pitch + x) * 4 + 3] = 0;
					else
						p_update_bgra[(y * pitch + x) * 4 + 3] = (unsigned char)bt_res;*/

					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
					reset = false;
				}
				if (gs->filter.gamma != 1.0f)
				{
					p_update_bgra[(y * pitch + x) * 4] = gammaValues[p_byte_bgra[idx]];
					p_update_bgra[(y * pitch + x) * 4 + 1] = gammaValues[p_byte_bgra[idx + 1]];
					p_update_bgra[(y * pitch + x) * 4 + 2] = gammaValues[p_byte_bgra[idx + 2]];

					p_byte_bgra = p_update_bgra;
					idx = (y * pitch + x) * 4;
					reset = false;
				}

				if (gs->filter.saturation != 0.0f)
				{
					gw = (p_byte_bgra[idx] + p_byte_bgra[idx + 1] + p_byte_bgra[idx + 2]) / 3.0f;

					sat_res = p_byte_bgra[idx] + ((p_byte_bgra[idx] - gw) * gs->filter.saturation);
					if (sat_res > 255)
						sat_res = 255;
					else if (sat_res < 0)
						sat_res = 0;
					p_update_bgra[(y * pitch + x) * 4] = sat_res;

					sat_res = p_byte_bgra[idx + 1] + ((p_byte_bgra[idx + 1] - gw) * gs->filter.saturation);
					if (sat_res > 255)
						sat_res = 255;
					else if (sat_res < 0)
						sat_res = 0;
					p_update_bgra[(y * pitch + x) * 4 + 1] = sat_res;

					sat_res = p_byte_bgra[idx + 2] + ((p_byte_bgra[idx + 2] - gw) * gs->filter.saturation);
					if (sat_res > 255)
						sat_res = 255;
					else if (sat_res < 0)
						sat_res = 0;
					p_update_bgra[(y * pitch + x) * 4 + 2] = sat_res;

					p_byte_bgra = p_update_bgra;
					reset = false;
				}

				if (reset)
				{
					p_update_bgra[(y * pitch + x) * 4] = p_byte_bgra[idx];
					p_update_bgra[(y * pitch + x) * 4 + 1] = p_byte_bgra[idx + 1];
					p_update_bgra[(y * pitch + x) * 4 + 2] = p_byte_bgra[idx + 2];
				}
				else
				{
					//convert alpha from straight to premultiplied
					if (gs->use_alpha && gs->alpha_premultiplied)
					{
						float mul = (float)p_byte_bgra[idx + 3] / 255.0f;
						p_update_bgra[(y * pitch + x) * 4] = (unsigned char)round((float)p_byte_bgra[idx] * mul);
						p_update_bgra[(y * pitch + x) * 4 + 1] = (unsigned char)round((float)p_byte_bgra[idx + 1] * mul);
						p_update_bgra[(y * pitch + x) * 4 + 2] = (unsigned char)round((float)p_byte_bgra[idx + 2] * mul);
					}
				}
			}
		}
	}
}

bool GFXEngine::_ApplyFilter(GFXSurface *gs_src, GFXFilter *apply_filter, bool render_manual)
{
	bool render_filter = false;

	if (apply_filter)
	{
		if (!_compareFilter(gs_src->filter.brightness,apply_filter->brightness))
		{
			gs_src->filter.brightness = apply_filter->brightness;
			render_filter = true;
		}
		if (!_compareFilter(gs_src->filter.contrast,apply_filter->contrast))
		{
			gs_src->filter.contrast = apply_filter->contrast;
			render_filter = true;
		}
		if (!_compareFilter(gs_src->filter.gamma, apply_filter->gamma))
		{
			gs_src->filter.gamma = apply_filter->gamma;
			render_filter = true;
		}
		if (gs_src->filter.invert != apply_filter->invert)
		{
			gs_src->filter.invert = apply_filter->invert;
			render_filter = true;
		}
		if (!_compareFilter(gs_src->filter.saturation, apply_filter->saturation))
		{
			gs_src->filter.saturation = apply_filter->saturation;
			render_filter = true;
		}
		if (!_compareFilter(gs_src->filter.shift_intensity, apply_filter->shift_intensity) || gs_src->filter.shift_color != apply_filter->shift_color)
		{
			gs_src->filter.shift_intensity = apply_filter->shift_intensity;
			gs_src->filter.shift_color = apply_filter->shift_color;
			render_filter = true;
		}

		if (render_filter)
		{
			if (!render_manual)
				_CopyColorInfo(gs_src, false, true);
			return true;
		}
	}
	return false;
}

bool GFXEngine::ApplyFilter(GFXSurface* gs) {

	if (!gs) {
		return false;
	}

	if (this->_ApplyFilter(gs, &gs->apply_filter)) {

		// copy rendered pixels to bgra memory
		if (this->_HasBGRABuffer(gs)) {
			memcpy(gs->bgra, p_update_bgra, 4 * gs->w * gs->h);
		}

		// reset filter settings
		gs->apply_filter = GFXFilter();
		gs->filter = gs->apply_filter;

		return true;
	}

	return false;
}

void GFXEngine::SetFilter_ColorShift(GFXSurface *gs, float intensity, unsigned int color)
{
	if(!gs)
		return;

	if(intensity < 0.0f)
		intensity = 0.0f;
	else if(intensity > 1.0f)
		intensity = 1.0f;

	gs->apply_filter.shift_intensity = intensity;
	gs->apply_filter.shift_color = color;
}

void GFXEngine::SetFilter_Brightness(GFXSurface *gs, float brightness)
{
	if(!gs)
		return;
		
	if(brightness < 0.0f)
		brightness = 0.0f;

	gs->apply_filter.brightness = brightness;
}

void GFXEngine::SetFilter_Gamma(GFXSurface *gs, float gamma)
{
	if(!gs)
		return;

	if(gamma < 0.1f)
		gamma = 0.1f;
	else if(gamma > 10.0f)
		gamma = 10.0f;

	gs->apply_filter.gamma = gamma;
}

void GFXEngine::SetFilter_Contrast(GFXSurface *gs, float contrast)
{
	if(!gs)
		return;
		
	if(contrast < 0.0f)
		contrast = 0.0f;

	gs->apply_filter.contrast = contrast;
}

void GFXEngine::SetFilter_Saturation(GFXSurface *gs, float saturation) // saturation -1.0 to 10.0, 0 = center
{
	if(!gs)
		return;

	if (saturation < -1.0f) {
		saturation = -1.0f;
	}

	if (saturation > 10.0f) {
		saturation = 10.0f;
	}
		
	gs->apply_filter.saturation = saturation;
}

void GFXEngine::SetFilter_Invert(GFXSurface *gs, bool invert)
{
	if(!gs)
		return;
		
	gs->apply_filter.invert = invert;
}

bool GFXEngine::_compareFilter(float filter1, float filter2) {

	float mul = powf(10, FILTER_ACCURACY);

	return (int)(filter1 * mul) == (int)(filter2 * mul);
}

bool GFXEngine::_equal(GFXFilter* filter1, GFXFilter* filter2, bool excludeHWFilter) {

	bool result = _compareFilter(filter1->contrast, filter2->contrast);
	result &= _compareFilter(filter1->gamma, filter2->gamma);
	result &= _compareFilter(filter1->contrast, filter2->contrast);
	result &= _compareFilter(filter1->saturation, filter2->saturation);
	result &= _compareFilter(filter1->shift_intensity, filter2->shift_intensity);
	result &= filter1->shift_color == filter2->shift_color;
	result &= filter1->invert == filter2->invert;

	if (!excludeHWFilter) {
		result &= _compareFilter(filter1->brightness, filter2->brightness);
	}

	return result;
}
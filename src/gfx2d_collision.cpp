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

bool GFXEngine::_CollisionBorderPointExists(GFXSurface *gs, int x, int y) {
	for (multimap<int, Point*>::iterator it = gs->borderPixel->lower_bound(x); it != gs->borderPixel->upper_bound(x + 1); ++it) {
		if (x == it->second->x && y == it->second->y)
			return true;
	}
	return false;
}

Vector2D* GFXEngine::_convertToScreen(Vector2D *v, float screenX, float screenY, Rect *rc, int flag, 
	float rotation, Vector2D* rotationOffset, float scaleX, float scaleY) {

	if (rc) {
		v->subtractX((float)rc->left);
		v->subtractY((float)rc->top);
	}

	if (flag & GFX_HFLIP) {
		if (rc) {
			v->setX((float)rc->getWidth() - v->getX());
		}
	}
	if (flag & GFX_VFLIP) {
		if (rc) {
			v->setY((float)rc->getHeight() - v->getY());
		}
	}

	Vector2D pos(screenX, screenY);
	Vector2D center(0.0f, 0.0f);
	if (rotationOffset) {
		center = *rotationOffset;
	}
	if (rc) {
		center.addX((float)rc->getWidth() / 2.0f);
		center.addY((float)rc->getHeight() / 2.0f);
		// zentrieren f�r rotation um mittelpunkt
		v->subtract(&center); // weltkordinaten auf 0/0 (zentrieren)
	}
	//umrechnung von relativ auf screen
	v->rotate(rotation); // rotieren
	v->add(&center); // pos auf screenzentrum setzen
	v->multiplyX(scaleX);
	v->multiplyY(scaleY);
	v->add(&pos); // relative koordinaten auf screenkoordianten schieben

	return v;
}

Vector2D* GFXEngine::_convertFromScreen(Vector2D *v, float objX, float objY, Rect *rc, int flag, float rotation, 
	Vector2D* rotationOffset, float scaleX, float scaleY) {
	// v in relative koordinaten von obj umrechnen
	Vector2D pos(objX, objY);
	Vector2D center(0.0f, 0.0f);
	if (rotationOffset) {
		center = *rotationOffset;
	}
	if (rc) {
		center.addX((float)rc->getWidth() / 2.0f);
		center.addY((float)rc->getHeight() / 2.0f);
	}
	v->subtract(&pos);
	v->multiplyX(1 / scaleX);
	v->multiplyY(1 / scaleY);
	v->subtract(&center);
	v->rotate(-rotation);
	v->add(&center);
	
	if (rc) {
		if (flag & GFX_HFLIP) {
			v->setX((float)rc->getWidth() - v->getX());
		}
		if (flag & GFX_VFLIP) {
			v->setY((float)rc->getHeight() - v->getY());
		}
		v->addX((float)rc->left);
		v->addY((float)rc->top);
	}

	return v;
}

bool GFXEngine::CreateCollisionData(GFXSurface *gs, bool borderCheck)
{
	if (!gs)
		return false;

	if (_HasBGRABuffer(gs)) {

		DeleteCollisionData(gs);

		if (gs->use_alpha)
		{
			gs->collision_array = new bool[gs->w * gs->h];
			if (!gs->collision_array)
			{
				return false;
			}
			int left = MAXINT32;
			int top = MAXINT32;
			int right = -1;
			int bottom = -1;
			for (int n = 0; n < gs->w * gs->h; n++)
			{
				unsigned char* p = (unsigned char*)&gs->bgra[n];
				gs->collision_array[n] = (p[3] & 0x80); // alpha >= 128

				int x = n % gs->w;
				int y = n / gs->w;

				// extrempunkte berechnen
				left = min(left, x);
				right = max(right, x);
				top = min(top, y);
				bottom = max(bottom, y);
			}

			if (borderCheck) {
				gs->borderPixel = new multimap<int, Point*>;

				//R�nder erkennen mit min und maxima pro vertikale und horizontale line
				deque<int> verSeparators; // Leere Zeilen um fehlenden vertikalcheck innerhalb der grafik durchzuf�hren
				deque<int> horSeparators; // s.o f�r horizontal

				//separatoren berechnen
				//vertikaler check
				verSeparators.push_back(left - 1); // linker Separator
				bool lastWasSeparator = false;
				for (int x = left + 1; x < right - 1; x++) {
					bool lineWasEmpty = true;
					for (int y = top; y < bottom; y++) {
						if (gs->collision_array[y * gs->w + x]) {
							lineWasEmpty = false;
							break;
						}
					}
					if (lineWasEmpty) {
						if (!lastWasSeparator) {
							verSeparators.push_back(x);
							lastWasSeparator = true;
						}
					}
					else {
						lastWasSeparator = false;
					}
				}
				verSeparators.push_back(right + 1); // rechter Separator
				// horizontaler check
				horSeparators.push_back(top - 1); // oberster Separator
				lastWasSeparator = false;
				for (int y = top + 1; y < bottom - 1; y++) {
					bool lineWasEmpty = true;
					for (int x = left; x < right; x++) {
						if (gs->collision_array[y * gs->w + x]) {
							lineWasEmpty = false;
							break;
						}
					}
					if (lineWasEmpty) {
						if (!lastWasSeparator) {
							horSeparators.push_back(y);
							lastWasSeparator = true;
						}
					}
					else {
						lastWasSeparator = false;
					}
				}
				horSeparators.push_back(bottom + 1); // unterer Separator

				for (deque<int>::iterator it = horSeparators.begin(); it != horSeparators.end();) {
					//	debugDrawLine(Vector2D(0, (*it)), Vector2D(300, 0), WHITE, 1);
					int start = (*it++) + 1;
					if (it == horSeparators.end())
						break;
					int end = (*it);

					// oben und unten
					for (int x = left; x < right; x++) {
						int minVal = MAXINT32;
						int maxVal = -1;
						for (int y = start; y < end; y++) {
							if (gs->collision_array[y * gs->w + x]) {
								minVal = min(minVal, y);
								maxVal = max(maxVal, y);
							}
						}

						if (minVal != MAXINT32) {// wenn pixel gefunden
							if (!_CollisionBorderPointExists(gs, x, minVal)) {
								Point* pt = new Point;
								pt->x = x;
								pt->y = minVal;
								gs->borderPixel->insert(pair<int, Point*>(x, pt));
							}
						}
						if (maxVal != -1) { // wenn pixel gefunden
							if (!_CollisionBorderPointExists(gs, x, maxVal)) {
								Point* pt = new Point;
								pt->x = x;
								pt->y = maxVal;
								gs->borderPixel->insert(pair<int, Point*>(x, pt));
							}
						}
					}
				}

				for (deque<int>::iterator it = verSeparators.begin(); it != verSeparators.end();) {
					//	debugDrawLine(Vector2D((*it), 0), Vector2D(0, 300), WHITE, 1);
					int start = (*it++) + 1;
					if (it == verSeparators.end())
						break;
					int end = (*it);

					for (int y = top; y < bottom; y++) {
						int minVal = MAXINT32;
						int maxVal = -1;
						for (int x = start; x < end; x++) {
							if (gs->collision_array[y * gs->w + x]) {
								minVal = min(minVal, x);
								maxVal = max(maxVal, x);
							}
						}

						if (minVal != MAXINT32) { // wenn pixel gefunden
							if (!_CollisionBorderPointExists(gs, minVal, y)) {
								Point* pt = new Point;
								pt->x = minVal;
								pt->y = y;
								gs->borderPixel->insert(pair<int, Point*>(minVal, pt));
							}
						}
						if (maxVal != -1) { // wenn pixel gefunden
							if (!_CollisionBorderPointExists(gs, maxVal, y)) {
								Point* pt = new Point;
								pt->x = maxVal;
								pt->y = y;
								gs->borderPixel->insert(pair<int, Point*>(maxVal, pt));
							}
						}
					}
				}

				// fehlende pixel auff�llen
				bool on = true;
				//horizontaler scan
				for (int x = left; x < right; x++) {
					for (int y = top; y < bottom; y++) {
						if (gs->collision_array[y * gs->w + x]) {
							if (on) {
								if (!_CollisionBorderPointExists(gs, x, y)) {
									Point* pt = new Point;
									pt->x = x;
									pt->y = y;
									gs->borderPixel->insert(pair<int, Point*>(x, pt));
								}
								on = false;
							}
						}
						else if (!on) {
							on = true;
							if (x - 1 > 0 && gs->collision_array[y * gs->w + x - 1]) {
								if (!_CollisionBorderPointExists(gs, x, y)) {
									Point* pt = new Point;
									pt->x = x;
									pt->y = y;
									gs->borderPixel->insert(pair<int, Point*>(x - 1, pt));
								}
							}
						}
					}
				}
				on = true;
				//vertikaler scan
				for (int y = top; y < bottom; y++) {
					for (int x = left; x < right; x++) {
						if (gs->collision_array[y * gs->w + x]) {
							if (on) {
								if (!_CollisionBorderPointExists(gs, x, y)) {
									Point* pt = new Point;
									pt->x = x;
									pt->y = y;
									gs->borderPixel->insert(pair<int, Point*>(x, pt));
								}
								on = false;
							}
						}
						else if (!on) {
							on = true;
							if (x - 1 > 0 && gs->collision_array[y * gs->w + x - 1]) {
								if (!_CollisionBorderPointExists(gs, x, y)) {
									Point* pt = new Point;
									pt->x = x;
									pt->y = y;
									gs->borderPixel->insert(pair<int, Point*>(x - 1, pt));
								}
							}
						}
					}
				}
			}
		}
		return true;
	}
	return false;
}

void GFXEngine::DeleteCollisionData(GFXSurface *gs)
{
	if (!gs)
		return;

	if (gs->collision_array)
	{
		delete[] gs->collision_array;
		gs->collision_array = NULL;
	}

	if (gs->borderPixel && gs->borderPixel->size()) {
		for (multimap<int, Point*>::iterator it = gs->borderPixel->begin(); it != gs->borderPixel->end(); ++it) {
			delete it->second;
			it->second = NULL;
		}
		gs->borderPixel->clear();
	}
}

bool GFXEngine::Collision(float pt_x, float pt_y, float r_x, float r_y, Rect *rc, float rotation, Vector2D *rotationOffset) {

	if (!rc) {
		return false;
	}

	Vector2D pt(pt_x, pt_y);
	_convertFromScreen(&pt, r_x, r_y, rc, 0, rotation, rotationOffset);

	if (pt.getX() >= (float)rc->left && pt.getX() < (float)rc->right && pt.getY() >= (float)rc->top && pt.getY() < (float)rc->bottom) {
		return true;
	}

	return false;
}

bool GFXEngine::Collision(const float x1, const float y1, const float radius1, const float x2, const float y2, const float radius2, Vector2D *colPt) {

	Vector2D v(x1 - x2, y1 - y2);
	if (v.length() < radius1 + radius2) {
		if (colPt) {
			*colPt = Vector2D(x2, y2);
			v.multiply(0.5);
			colPt->add(&v);
		}
		return true;
	}

	return false;
}


bool GFXEngine::Collision(float pt_x, float pt_y, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, 
	float gs_rotation, Vector2D* rotationOffset, float scale) {

	Vector2D* pStretch = NULL;
	Vector2D stretch;
	if (gs) {
		if (gs_rc) {
			stretch.set((float)gs_rc->getWidth() * scale, (float)gs_rc->getHeight() * scale);
		}
		else {
			stretch.set((float)gs->w * scale, (float)gs->h * scale);
		}
		pStretch = &stretch;
	}
	return Collision(pt_x, pt_y, gs, gs_x, gs_y, gs_rc, flags, gs_rotation, rotationOffset, pStretch);
}

bool GFXEngine::Collision(float pt_x, float pt_y, GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, 
	float gs_rotation, Vector2D* rotationOffset, Vector2D* stretch) {

	if (!gs || !gs->collision_array) { // keine grafik -> punkt in rechteck

		if (stretch) {
			Rect rc = {0, 0, (int)round(stretch->getX()), (int)round(stretch->getY())};
			return Collision(pt_x, pt_y, gs_x, gs_y, &rc, gs_rotation, rotationOffset);
		}
		return Collision(pt_x, pt_y, gs_x, gs_y, gs_rc, gs_rotation, rotationOffset);
	}

	Rect rc(0, 0, gs->w, gs->h);
	if (gs_rc) {
		rc = *gs_rc;
	}

	float scaleX = 1.0f;
	float scaleY = 1.0f;
	if (stretch) {
		scaleX = stretch->getX() / (float)rc.getWidth();
		scaleY = stretch->getY() / (float)rc.getHeight();
	}

	//auf rect-bereich umrechnen
	Vector2D pt(pt_x, pt_y);
	_convertFromScreen(&pt, gs_x, gs_y, gs_rc, flags, gs_rotation, rotationOffset, scaleX, scaleY);

	int x = (int)round(pt.getX());
	int y = (int)round(pt.getY());

	if (x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom) {
		return (gs->collision_array[x + (y * gs->w)]);
	}
	return false;
}

bool GFXEngine::Collision(float x1, float y1, Rect *rc1, float rotation1, Vector2D* rotationOffset1,
	float x2, float y2, Rect *rc2, float rotation2, Vector2D* rotationOffset2, Vector2D *colPt) {

	if (!rc1 || !rc2)
		return false;

	//	debugDrawLine(center2, Vector2D(2, 2), RGB(0, 255, 255), 3, Vector2D(x1, y1));
	//	debugDrawLine(pos2, Vector2D(2,2), RGB(0, 0, 255), 3, Vector2D(x1, y1));

	Vector2D r2Comp[4];
	r2Comp[0] = Vector2D((float)rc2->left, (float)rc2->top);
	r2Comp[1] = Vector2D((float)rc2->right, (float)rc2->top);
	r2Comp[2] = Vector2D((float)rc2->right, (float)rc2->bottom);
	r2Comp[3] = Vector2D((float)rc2->left, (float)rc2->bottom);
	_convertToScreen(&r2Comp[0], x2, y2, rc2, 0, rotation2, rotationOffset2);
	_convertToScreen(&r2Comp[1], x2, y2, rc2, 0, rotation2, rotationOffset2);
	_convertToScreen(&r2Comp[2], x2, y2, rc2, 0, rotation2, rotationOffset2);
	_convertToScreen(&r2Comp[3], x2, y2, rc2, 0, rotation2, rotationOffset2);
	_convertFromScreen(&r2Comp[0], x1, y1, rc1, 0, rotation1, rotationOffset1);
	_convertFromScreen(&r2Comp[1], x1, y1, rc1, 0, rotation1, rotationOffset1);
	_convertFromScreen(&r2Comp[2], x1, y1, rc1, 0, rotation1, rotationOffset1);
	_convertFromScreen(&r2Comp[3], x1, y1, rc1, 0, rotation1, rotationOffset1);

	//schneidet
	Vector2D r1Comp[4];
	r1Comp[0] = Vector2D((float)rc1->left, (float)rc1->top);
	r1Comp[1] = Vector2D((float)rc1->right, (float)rc1->top);
	r1Comp[2] = Vector2D((float)rc1->right, (float)rc1->bottom);
	r1Comp[3] = Vector2D((float)rc1->left, (float)rc1->bottom);

	// Debug Anzeige
/*	for (int a = 0; a < 4; a++) {
		for (int b = 0; b < 4; b++) {
			Vector2D v1 = r2Comp[b];
			_convertToScreen(&v1, x1, y1, rc1, 0, rotation1);
			Vector2D v2 = r2Comp[(b + 1) % 4];
			_convertToScreen(&v2, x1, y1, rc1, 0, rotation1);
			if(debugScroll)
				debugDrawLine(v1, *v2.subtract(&v1), RGB(100, 100, 200), 3, *debugScroll);
		}
		Vector2D v1 = r1Comp[a];
		_convertToScreen(&v1, x1, y1);
		Vector2D v2 = r1Comp[(a + 1) % 4];
		_convertToScreen(&v2, x1, y1);
		if (debugScroll)
			debugDrawLine(v1, *v2.subtract(&v1), RGB(100, 100, 200), 3, *debugScroll);
	}*/

	for (int a = 0; a < 4; a++) {
		for (int b = 0; b < 4; b++) {
			if (Vector2D::intersects(&r1Comp[b], &r1Comp[(b + 1) % 4], &r2Comp[a], &r2Comp[(a + 1) % 4], colPt)) {
				if (colPt) {
					_convertToScreen(colPt, x1, y1, rc1, 0, rotation1, rotationOffset1);
				}
				return true;
			}
		}
	}

	//R2 komplett innerhalb
	// Pr�fe ob einer der Punkte in R1 liegt
	for (int b = 0; b < 4; b++) {
		if (Collision(r2Comp[b].getX(), r2Comp[b].getY(), 0, 0, rc1)) {
			if (colPt) {
				*colPt = r2Comp[b];
				_convertToScreen(colPt, x1, y1, rc1, 0, rotation1, rotationOffset1);
			}
			return true;
		}
	}

	//R2 schlie�t R1 komplett ein
	//pr�fe ob einer der Punkte von R1 rechts von alle Vektoren von R2 liegt (uhrzeigersinn)
	bool col = true;
	for (int b = 0; b < 4; b++) {
		if (Vector2D::leftRightTest(&r2Comp[b], &r2Comp[(b + 1) % 4], &r1Comp[0]) < 0) {
			col = false;
		}
		//		debugDrawLine(r2Comp[b], *v.subtract(&r2Comp[b]), RGB(255, 0, 0), 3, Vector2D(x1, y1));
	}
	if (colPt) {
		*colPt = Vector2D(x1 + (float)rc1->getWidth() / 2.0f, y1 + (float)rc1->getHeight() / 2.0f);
	}

	return col;
}

bool GFXEngine::Collision(GFXSurface* gs1, float x1, float y1, Rect* rc1, int flags1, float rotation1, Vector2D* rotationOffset1, float scale1,
	GFXSurface* gs2, float x2, float y2, Rect* rc2, int flags2, float rotation2, Vector2D* rotationOffset2, float scale2, Vector2D* pt_col)
{
	Vector2D stretch1;
	if (rc1) {
		stretch1.set((float)rc1->getWidth() * scale1, (float)rc1->getHeight() * scale1);
	}
	else if (gs1) {
		stretch1.set((float)gs1->w * scale1, (float)gs1->h * scale1);
	}
	else {
		return false;
	}

	Vector2D stretch2;
	if (rc2) {
		stretch2.set((float)rc2->getWidth() * scale2, (float)rc2->getHeight() * scale2);
	}
	else if (gs2) {
		stretch2.set((float)gs2->w * scale2, (float)gs2->h * scale2);
	}
	else {
		return false;
	}

	return Collision(gs1, x1, y1, rc1, flags1, rotation1, rotationOffset1, &stretch1, 
		gs2, x2, y2, rc2, flags2, rotation2, rotationOffset2, &stretch2, pt_col);
}

bool GFXEngine::Collision(GFXSurface* gs1, float x1, float y1, Rect* rc1, int flags1, float rotation1, Vector2D* rotationOffset1, Vector2D *stretch1,
	GFXSurface* gs2, float x2, float y2, Rect* rc2, int flags2, float rotation2, Vector2D* rotationOffset2, Vector2D *stretch2, Vector2D *pt_col)
{
	bool newrc1 = false, newrc2 = false;
	if (!rc1)
	{
		if (!gs1) {
			return false;
		}
		newrc1 = true;
		rc1 = new Rect;
		rc1->left = 0;
		rc1->right = gs1->w;
		rc1->top = 0;
		rc1->bottom = gs1->h;
	}
	if (!rc2)
	{
		if (!gs2)
		{
			if (newrc1)
				delete rc1;
			return false;
		}
		newrc2 = true;
		rc2 = new Rect;
		rc2->left = 0;
		rc2->right = gs2->w;
		rc2->top = 0;
		rc2->bottom = gs2->h;
	}

	//pt in rect check

	bool collision = false;

	Vector2D frame1(rc1 ? (float)rc1->getWidth() : 0.0f, rc1 ? (float)rc1->getHeight() : 0.0f);
	if (stretch1) {
		frame1.setX(max(frame1.getX(), stretch1->getX()));
		frame1.setY(max(frame1.getY(), stretch1->getY()));
	}
	float radius1 = frame1.length();

	Vector2D frame2(rc2 ? (float)rc2->getWidth() : 0.0f, rc2 ? (float)rc2->getHeight() : 0.0f);
	if (stretch2) {
		frame2.setX(max(frame2.getX(), stretch2->getX()));
		frame2.setY(max(frame2.getY(), stretch2->getY()));
	}
	float radius2 = frame2.length();

	//radialer collisionscheck (performanter)
	if (Collision(x1 + frame1.getX() / 2.0f, y1 + frame1.getY() / 2.0f, radius1, 
		x2 + frame2.getX() / 2.0f, y2 + frame2.getY() / 2.0f, radius2, pt_col))
	{
		Rect rcheck1 = *rc1;
		Rect rcheck2 = *rc2;
		if (stretch1) {
			rcheck1.set(0, 0, (int)round(stretch1->getX()), (int)round(stretch1->getY()) );
		}
		if (stretch2) {
			rcheck2.set( 0, 0, (int)round(stretch2->getX()), (int)round(stretch2->getY()) );
		}

		if (Collision(x1, y1, &rcheck1, rotation1, rotationOffset1, x2, y2, &rcheck2, rotation2, rotationOffset2, pt_col)) // rect-in-rect-check
		{
			if ((!gs1 || !gs1->collision_array) && (!gs2 || !gs2->collision_array)) {
				return true;
			}
			
			// sortieren: falls gs1 NULL/kein CollisionData oder mehr kollisionspunkte hat als g2, tauschen
			if (!gs1 || !gs1->collision_array || (gs2 && gs2->borderPixel && gs1->borderPixel->size() > gs2->borderPixel->size())) {
				GFXSurface *gs = gs1;
				float x = x1;
				float y = y1;
				Rect *rc = rc1;
				int flag = flags1;
				float rotation = rotation1;
				Vector2D* rotationOffset = rotationOffset1;
				bool newrc = newrc1;
				Vector2D *stretch = stretch1;

				gs1 = gs2;
				x1 = x2;
				y1 = y2;
				rc1 = rc2;
				flags1 = flags2;
				rotation1 = rotation2;
				rotationOffset1 = rotationOffset2;
				stretch1 = stretch2;
				newrc1 = newrc2;

				gs2 = gs;
				x2 = x;
				y2 = y;
				rc2 = rc;
				flags2 = flag;
				rotation2 = rotation;
				rotationOffset2 = rotationOffset;
				stretch2 = stretch;
				newrc2 = newrc;
			}

			float scaleX1 = 1.0f;
			float scaleY1 = 1.0f;
			if (stretch1) {
				scaleX1 = stretch1->getX() / (float)rc1->getWidth();
				scaleY1 = stretch1->getY() / (float)rc1->getHeight();
			}
			float scaleX2 = 1.0f;
			float scaleY2 = 1.0f;
			if (stretch2) {
				scaleX2 = stretch2->getX() / (float)rc2->getWidth();
				scaleY2 = stretch2->getY() / (float)rc2->getHeight();
			}

			if (gs1->borderPixel) {
				Vector2D vCol(0.0f, 0.0f);
				int points = 0;
				for (multimap<int, Point*>::iterator it = gs1->borderPixel->lower_bound(rc1->left); it != gs1->borderPixel->upper_bound(rc1->right); ++it) {
					if (it->second->y < rc1->top || it->second->y >= rc1->bottom)
						continue;

					Vector2D v1((float)it->second->x, (float)it->second->y);
					_convertToScreen(&v1, x1, y1, rc1, flags1, rotation1, rotationOffset1, scaleX1, scaleY1);

					_convertFromScreen(&v1, x2, y2, rc2, flags2, rotation2, rotationOffset2, scaleX2, scaleY2);

					if (gs2 && gs2->collision_array) { // pixelcheck

						//auf rect-bereich umrechnen
						int x = (int)round(v1.getX()) ;
						int y = (int)round(v1.getY());

						if (x >= rc2->left && x < rc2->right && y >= rc2->top && y < rc2->bottom) {

							if (gs2->collision_array[x + (y * gs2->w)]) {
								if (!pt_col)
									return true;
								collision = true;
								vCol.addX(v1.getX());
								vCol.addY(v1.getY());
								points++;
							}
						}
					}
					else { // pt in rect check
						if (stretch2) {
							Rect rc(0, 0, (int)round(stretch2->getX()), (int)round(stretch2->getY()));
							if (Collision(v1.getX(), v1.getY(), 0.0f, 0.0f, &rc)) {
								if (!pt_col)
									return true;
								collision = true;
								vCol.addX(v1.getX());
								vCol.addY(v1.getY());
								points++;
							}
						}
						else {
							if (Collision(v1.getX(), v1.getY(), 0.0f, 0.0f, rc2)) {
								if (!pt_col)
									return true;
								collision = true;
								vCol.addX(v1.getX());
								vCol.addY(v1.getY());
								points++;
							}
						}
					}
				}

				if (collision) {
					if (pt_col) {
						if (points) {
							vCol.multiply(1.0f / (float)points);
						}
						_convertToScreen(&vCol, x2, y2, rc2, flags2, rotation2, rotationOffset2, scaleX2, scaleY2);
						*pt_col = vCol;
					}
					return true;
				}
			}

			//Prüfe ob r2 ganz in r1 liegt: hier reicht es einen punkt zu prüfen, da kein schnitt gefunden wurde
			Vector2D v2(x2, y2);
			if (gs2 && gs2->borderPixel) { // lese pixel für obj2 aus tree, falls keine collisiondata vorhanden nutze punkt x2,y2 (screen) bzw. 0,0 (relativ)

				for (multimap<int, Point*>::iterator it = gs2->borderPixel->lower_bound(rc2->left); it != gs2->borderPixel->upper_bound(rc2->right); ++it) {
					if (it->second->y >= rc2->top && it->second->y < rc2->bottom) { // suche einen borderpixel im geeigneten bereich

						v2.set((float)it->second->x, (float)it->second->y);
						_convertToScreen(&v2, x2, y2, rc2, flags2, rotation2, rotationOffset2, scaleX2, scaleY2);

						break;
					}
				}
			}
			//	if (pt_col)
			//		*pt_col = v2;

			_convertFromScreen(&v2, x1, y1, rc1, flags1, rotation1, rotationOffset1, scaleX1, scaleY1);

			//auf rect-bereich umrechnen
			int x = (int)round(v2.getX());
			int y = (int)round(v2.getY());

			Vector2D vtest = v2;
			_convertToScreen(&vtest, x1, y1, rc1, flags1, rotation1, rotationOffset1, scaleX1, scaleY1);

			if (x >= rc1->left && x < rc1->right && y >= rc1->top && y < rc1->bottom) {

				if (gs1->collision_array[x + (y * gs1->w)]) {
					return true;
				}
			}
		}
	}

	if (newrc1) {
		delete rc1;
	}
	if (newrc2) {
		delete rc2;
	}

	return false;
}

Vector2D GFXEngine::GetTangentAt(float pt_x, float pt_y, float direction,
	GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, float rotation, Vector2D* rotationOffset, float scale, Vector2D* borderColPt) {

	Vector2D stretch;
	if (gs_rc) {
		stretch.set((float)gs_rc->getWidth() * scale, (float)gs_rc->getHeight() * scale);
	}
	else if (gs) {
		stretch.set((float)gs->w * scale, (float)gs->h * scale);
	}
	else {
		return false;
	}

	return GetTangentAt(pt_x, pt_y, direction, gs, gs_x, gs_y, gs_rc, flags, rotation, rotationOffset, &stretch, borderColPt);
}

Vector2D GFXEngine::GetTangentAt(float pt_x, float pt_y, float direction,
	GFXSurface* gs, float gs_x, float gs_y, Rect* gs_rc, int flags, float rotation, Vector2D* rotationOffset, Vector2D* stretch, Vector2D *borderColPt) {
	Vector2D v_tangent(0, 0);

	Vector2D v_pt(pt_x, pt_y);
	Vector2D v_direction = Vector2D::Vector2DFromAngle(direction, 1.0f);
	float rot = rotation;
	
	v_direction.rotate(-rot);
	if (flags & GFX_HFLIP) {
		v_direction.multiplyX(-1.0f);
	}
	if (flags & GFX_VFLIP) {
		v_direction.multiplyY(-1.0f);
	}

	Rect rc_frame(0, 0, gs->w, gs->h);
	if (gs_rc) {
		rc_frame = *gs_rc;
	}

	float scaleX = 1.0f;
	float scaleY = 1.0f;
	if (stretch) {
		scaleX = stretch->getX() / (float)rc_frame.getWidth();
		scaleY = stretch->getY() / (float)rc_frame.getHeight();
	}

	_convertFromScreen(&v_pt, gs_x, gs_y, gs_rc, flags, rotation, rotationOffset, scaleX, scaleY);

	//finde nächstgelegenen borderPixel
	Point pt;
	pt.x = (int)round(v_pt.getX());
	pt.y = (int)round(v_pt.getY());

	int col_tan_area = COLLISION_TANGENT_AREA;
	Vector2D tangent_pt_left(v_pt), tangent_pt_right(v_pt);

	//debug
/*	for (multimap<int, Point*>::iterator it = gs->borderPixel->lower_bound(rc_frame.left); it != gs->borderPixel->upper_bound(rc_frame.right); ++it) {
		if (it->second->y < rc_frame.top || it->second->y >= rc_frame.bottom)
			continue;
		_debugDrawLine(Vector2D(100 + it->second->x * 2, 100 + it->second->y * 2), Vector2D(2, 0), RGB(100,100,100), 2);
	}
*/	
	while (tangent_pt_left.equals(&tangent_pt_right) && col_tan_area <= max(gs->w / 2, gs->h / 2)) { // solange keine tangente gefunden wurde

		Rect rc = { pt.x - col_tan_area, pt.y - col_tan_area, pt.x + col_tan_area, pt.y + col_tan_area };
		if (gs_rc) {
			rc.left = max(rc.left, gs_rc->left);
			rc.top = max(rc.top, gs_rc->top);
			rc.right = min(rc.right, gs_rc->right);
			rc.bottom = min(rc.bottom, gs_rc->bottom);
		}

		for (multimap<int, Point*>::iterator it = gs->borderPixel->lower_bound(rc.left); it != gs->borderPixel->upper_bound(rc.right); ++it) {
			if (it->second->y < rc.top || it->second->y >= rc.bottom)
				continue;

			if (it->second->x == pt.x && it->second->y == pt.y) // collisions border-pixel ausschließen
				continue;

			// punkte hinter den kollisionspunkt ausschließen
			Vector2D v_direction90 = v_direction;
			v_direction90.rotate(M_PI_2F);
			Vector2D v1(v_pt.getX() + v_direction90.getX(), v_pt.getY() + v_direction90.getY());
			Vector2D v2((float)it->second->x, (float)it->second->y);
			if (Vector2D::leftRightTest(&v_pt, &v1, &v2) < 0) {
				continue;
			}

			/*	_debugDrawLine(Vector2D(100 + v_pt.getX() * 2, 100 + v_pt.getY() * 2),
					Vector2D(v_direction90.getX() * 100, v_direction90.getY() * 100), RGB(100, 255, 100), 2);

				_debugDrawLine(Vector2D(100 + v_pt.getX() * 2, 100 + v_pt.getY() * 2),
					Vector2D(v_direction.getX() * 100, v_direction.getY() * 100), RGB(100, 255, 255), 2);
					*/

			v1.set(tangent_pt_left.getX() - v_direction.getX(), tangent_pt_left.getY() - v_direction.getY());
			v2.set((float)it->second->x, (float)it->second->y);
			if (Vector2D::leftRightTest(&tangent_pt_left, &v1, &v2) > 0) { // wenn links von der geraden

				tangent_pt_left = Vector2D((float)it->second->x, (float)it->second->y);
			}
			else if (Vector2D::leftRightTest(&tangent_pt_right, &v1, &v2) <= 0) { // wenn rechts von der geraden

				tangent_pt_right = Vector2D((float)it->second->x, (float)it->second->y);
			}
		}
		col_tan_area += COLLISION_TANGENT_AREA;
	}

	if ((flags & GFX_HFLIP && !(flags & GFX_VFLIP)) || (!(flags & GFX_HFLIP) && flags & GFX_VFLIP)) {
		v_tangent = tangent_pt_left;
		v_tangent.subtract(&tangent_pt_right);
	}
	else {
		v_tangent = tangent_pt_right;
		v_tangent.subtract(&tangent_pt_left);
	}

	if (flags & GFX_HFLIP) {
		v_tangent.multiplyX(-1.0f);
	}
	if (flags & GFX_VFLIP) {
		v_tangent.multiplyY(-1.0f);
	}

	/*
	_debugDrawLine(Vector2D(100 + (tangent_pt_left.getX() + v_tangent.getX() / 2) * 2,
		100 + (tangent_pt_left.getY() + v_tangent.getY() / 2) * 2), Vector2D(2, 0), RGB(0, 255, 0), 2);

	_debugDrawLine(Vector2D(100 + tangent_pt_right.getX() * 2, 100 + tangent_pt_right.getY() * 2), Vector2D(2, 0), RGB(255, 0, 0), 2);
	_debugDrawLine(Vector2D(100 + tangent_pt_left.getX() * 2, 100 + tangent_pt_left.getY() * 2), Vector2D(2, 0), RGB(255, 0, 0), 2);
	*/

	v_tangent.rotate(rot);
	return v_tangent;
}

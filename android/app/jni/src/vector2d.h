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

#ifndef _VECTOR2D_H_
#define _VECTOR2D_H_

#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#define DEG2RAD(x)	((x)*0.017453292f)
#define RAD2DEG(x)	((x)*57.295779513f)

#define M_PIF		(3.141592653589793238462643383279502884e+00f)
#define M_PI_2F		((M_PIF) / 2.0f)
#define M_PI_4F		((M_PIF) / 4.0f)

class Vector2D {
private:
	float x;
	float y;
	float storedLength;
	float storedAngle;
	void invalidate();
public:
	Vector2D(float x = 0, float y = 0);

	static Vector2D Vector2DFromAngle(float angle, float length);
	static float angleAdd(float *angle, float add);

	void set(float x, float y);
	void setX(float x);
	void setY(float y);
	float getX();
	float getY();
	float getAngle();				// Steigungswinkel des Vektors im Bogenmaß
	float getAngle(Vector2D* v);	// Winkel zwischen 2 Vektoren im Bogenmaß
	Vector2D* rotate(float angle);	// Winkel im Bogenmaß
	Vector2D* normalize();
	Vector2D* invert();
	Vector2D* multiply(float mul);
	Vector2D* multiplyX(float mul);
	Vector2D* multiplyY(float mul);
	Vector2D* add(Vector2D* v);
	Vector2D* addX(float x);
	Vector2D* addY(float y);
	Vector2D* subtract(Vector2D* v);
	Vector2D* subtractX(float x);
	Vector2D* subtractY(float y);
	Vector2D* reflect(Vector2D* v);
	float length();
	bool equals(Vector2D *v);
	static bool intersects(Vector2D *startPos1, Vector2D *endPos1, Vector2D *startPos2, Vector2D *endPos2, Vector2D *intersection=NULL);
	static float leftRightTest(Vector2D *a, Vector2D *b, Vector2D *c);
};

#endif
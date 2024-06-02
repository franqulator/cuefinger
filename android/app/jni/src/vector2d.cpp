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

#include "vector2d.h"

Vector2D::Vector2D(float x, float y) {
    this->x = x;
    this->y = y;
	this->invalidate();
}

void Vector2D::invalidate() {
	this->storedAngle = NAN;
	this->storedLength = NAN;
}

Vector2D Vector2D::Vector2DFromAngle(float angle, float length) {
	if (length == 0) {
		return Vector2D(0, 0);
	}
	Vector2D v;
	v.set(cosf(angle) * length, sinf(angle) * length);
	return v;
}

float Vector2D::angleAdd(float *angle, float add) { // Rückgabewert zwischen: -M_PI bis M_PI
	if(!angle) {
		return NAN;
	}

	*angle += add;

	*angle = fmodf(*angle, (2 * M_PIF));

	if(*angle > M_PIF) {
		*angle = *angle - 2 * M_PIF;
	}

	if(*angle < -M_PIF) {
		*angle =  *angle + 2 * M_PIF;
	}

	return *angle;
}
/*
float Vector2D::angleCompare(float a, float b) { // positiv wenn a größer b
	a = fmodf(a, (2 * M_PIF));
	b = fmodf(b, (2 * M_PIF));
	return a - b;
}*/

float Vector2D::getX() {
	return this->x;
}


float Vector2D::getY() {
	return this->y;
}

void Vector2D::set(float x, float y) {
	this->x = x;
	this->y = y;
	this->invalidate();
}

void Vector2D::setX(float x) {
	this->x = x;
	this->invalidate();
}

void Vector2D::setY(float y) {
	this->y = y;
	this->invalidate();
}

float Vector2D::getAngle() { // kartesisches Koordinatensystem
	if(!isnan(this->storedAngle)) {
		return this->storedAngle;
	}

	this->storedAngle = atan2(this->y, this->x);

	return this->storedAngle;
}

float Vector2D::getAngle(Vector2D* v) { // kartesisches Koordinatensystem
	if(!v)
		return NAN;

	float angle = v->getAngle();
	return Vector2D::angleAdd(&angle, -this->getAngle());
}

Vector2D* Vector2D::rotate(float angle) {

	if (angle == 0)
		return this;

	Vector2D vResult;

//	angle = fmodf(angle, (2 * M_PIF));

	float cosAngle = cosf(angle);
	float sinAngle = sinf(angle);

	vResult.x = this->x * cosAngle - this->y * sinAngle;
	vResult.y = this->x * sinAngle + this->y * cosAngle;

	this->x = vResult.x;
	this->y = vResult.y;

	this->invalidate();

	return this;
}

Vector2D* Vector2D::normalize() {

	if(this->length() == 0) {
		this->x = 0;
		this->y = 0;
	}
	else {
		this->x /= this->length();
		this->y /= this->length();
	}
	this->invalidate();

	return this;
}

Vector2D* Vector2D::invert() {

	this->x = -this->x;
	this->y = -this->y;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::multiply(float mul) {
	this->x *= mul;
	this->y *= mul;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::multiplyX(float mul) {

	this->x *= mul;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::multiplyY(float mul) {

	this->y *= mul;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::add(Vector2D* v) {
	if(!v)
		return NULL;

	this->x += v->x;
	this->y += v->y;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::addX(float x) {
	this->x += x;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::addY(float y) {
	this->y += y;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::subtract(Vector2D* v) {
	if(!v)
		return NULL;

	this->x -= v->x;
	this->y -= v->y;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::subtractX(float x) {
	this->x -= x;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::subtractY(float y) {
	this->y -= y;
	this->invalidate();

	return this;
}

Vector2D* Vector2D::reflect(Vector2D* v) { // berechnet den Ausgangsvektor bei einer Reflektion von this an v
	if(!v)
		return NULL;

	float angleIn = v->getAngle(this);
	float angleSf = v->getAngle();
	float angleOut = - angleIn + angleSf;
	*this = Vector2DFromAngle(angleOut, this->length());
	this->invalidate();

	return this;
}

float Vector2D::length() {
	if(!isnan(this->storedLength)) {
		return this->storedLength;
	}

	this->storedLength = sqrtf(powf(this->x, 2.0f) + powf(this->y, 2.0f));
	return this->storedLength;
}

bool Vector2D::equals(Vector2D *v) {
	if(!v)
		return false;

//	if (isnan(this->getX()) && isnan(v->getX()) && isnan(this->getY()) && isnan(v->getY()))
//		return true;

	return (this->x == v->getX() && this->y == v->getY());
}

bool Vector2D::intersects(Vector2D *startPos1, Vector2D *endPos1, Vector2D *startPos2, Vector2D *endPos2, Vector2D *intersection) {

	float x1 = startPos1->getX();
	float y1 = startPos1->getY();

	float x2 = endPos1->getX();
	float y2 = endPos1->getY();

	float x3 = startPos2->getX();
	float y3 = startPos2->getY();
	float x4 = endPos2->getX();
	float y4 = endPos2->getY();

	float den = ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
	float ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / den;
	float ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / den;

	float x = x1 + ua * (x2 - x1);
	float y = y1 + ua * (y2 - y1);

	if (intersection) {
		*intersection = Vector2D(x, y);
	}

	return ua >= 0 && ua <= 1 && ub >= 0 && ub <= 1;
}

/**
* Gibt zurück ob sich Punkt c links, rechts von oder auf der Geraden a-b befindet.
* @param a
* Punkt a
* @param b
* Punkt b
* @param c
* Punkt c
* @return
* > 0 für links, < 0  für rechts, 0 für kollinear
*/
float Vector2D::leftRightTest(Vector2D *a, Vector2D *b, Vector2D *c) {

	float r1 = a->getX() * (b->getY() - c->getY());
	float r2 = b->getX() * (c->getY() - a->getY());
	float r3 = c->getX() * (a->getY() - b->getY());

	return r1 + r2 + r3;
}
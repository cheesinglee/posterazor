/*
	PosteRazor - Make your own poster!
	Copyright (C) 2005 by Alessandro Portale
	http://posterazor.sourceforge.net/

	This file is part of PosteRazor

	PosteRazor is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	PosteRazor is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with PosteRazor; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef Fl_Gl_Paint_Canvas_h
#define Fl_Gl_Paint_Canvas_h

#include <FL/Fl_Gl_Window.H>

#include <FL/Fl_Image.H>
#include "../../tools/PaintCanvasInterface.h"

class Fl_Gl_Paint_Canvas: public Fl_Gl_Window, public PaintCanvasInterface
{
private:
	char m_stateString[1024];
	unsigned char *m_imageRGBData;
	Fl_RGB_Image *m_image;
	unsigned char m_backgroundColor[3];
	
public:
	Fl_Gl_Paint_Canvas(int width, int height, int x, int y);
	~Fl_Gl_Paint_Canvas();

	virtual void SetBackgroundColor(unsigned char red, unsigned char green, unsigned char blue);

	virtual void SetState(const char *state);
		
	virtual void draw();
	virtual void DrawFilledRect(double x, double y, double width, double height, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
	virtual void DrawRect(double x, double y, double width, double height, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
	virtual void DrawLine(double x1, double y1, double x2, double y2, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
	virtual void GetSize(double &width, double &height) const;

	virtual void RequestImage(void);
	virtual void SetImage(const unsigned char* rgbData, double width, double height);
	virtual void DisposeImage(void);
	virtual void DrawImage(double x, double y, double width, double height);
};

#endif Fl_Gl_Paint_Canvas_h
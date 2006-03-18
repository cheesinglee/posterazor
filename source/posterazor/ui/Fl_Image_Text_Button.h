/*
	PosteRazor - Make your own poster!
	Copyright (C) 2005-2006 by Alessandro Portale
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

#ifndef Fl_Image_Text_Button_h
#define Fl_Image_Text_Button_h

#include <Fl/Fl_Image.h>
#include <Fl/Fl_Button.h>

class Fl_Image_Text_Button: public Fl_Button
{
public:
	Fl_Image_Text_Button(int x, int y, int w, int h, const char* label = 0);
	void label_image(Fl_Image *image);

private:
	Fl_Image *m_label_image;

	virtual void draw();
};

#endif
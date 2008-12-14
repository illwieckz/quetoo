/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef __IMAGES_H__
#define __IMAGES_H__

#include <SDL.h>

#include "common.h"

// 8bit palette for wal images and particles
extern unsigned palette[256];

/*
Img_LoadImage

Loads the specified image from the game filesystem and populates
the provided SDL_Surface.

Image formats are tried in the order they appear in TYPES.
*/
qboolean Img_LoadImage(char *name, SDL_Surface **surf);

/*
Img_LoadTypedImage

Loads the specified image from the game filesystem and populates
the provided SDL_Surface.
*/
qboolean Img_LoadTypedImage(char *name, char *type, SDL_Surface **surf);

/*
Img_InitPalette

Initializes the 8bit color palette required for .wal texture loading.
*/
void Img_InitPalette(void);

/*
Img_ColorFromPalette

Returns RGB components of the specified color in the specified result array.
*/
void Img_ColorFromPalette(byte c, float *res);

/*
Img_WriteTGARLE

Write pixel data to a Type 10 (RLE compressed RGB) Targa file.
*/
void Img_WriteTGARLE(char *filename, void *data, int width, int height);

#endif /*__IMAGES_H__*/

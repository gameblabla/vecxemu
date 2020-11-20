/* Frontend for Vecx
 * MIT License, Copyright 2019 Gameblabla
 * */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>

#include <sys/time.h>
#include <sys/types.h>

#include "e6809.h"
#include "e8910.h"
#include "vecx.h"

#include "video_blit.h"
#include "config.h"

#ifndef NOROTATE
#error "Rotation not supported by GCW0 backend yet"
#endif

SDL_Surface *sdl_screen, *backbuffer;

uint32_t width_of_surface;
uint16_t* Draw_to_Virtual_Screen;

SDL_Surface *overlay;

static long scl_factor;
static long offx;
static long offy;

static long sclx, scly;

static int32_t screenx, screeny;

static uint16_t* restrict source_graph;
static uint32_t source_w, source_h, source_pitch;

void Set_resolution_vector()
{
	source_graph = sdl_screen->pixels;
	source_w = sdl_screen->w;
	source_h = sdl_screen->h;
	source_pitch = sdl_screen->pitch;
			
	sclx = (ALG_MAX_X / sdl_screen->w);
	scly = (ALG_MAX_Y / sdl_screen->h);

	scl_factor = sclx > scly ? sclx : scly;

	offx = (sdl_screen->w - ALG_MAX_X / scl_factor) / 2;
	offy = (sdl_screen->h - ALG_MAX_Y / scl_factor) / 2;
	
	SDL_FillRect(sdl_screen, NULL, 0);
}

void setPixel(uint_fast32_t x, uint_fast32_t y, uint32_t pixel)
{
	if (x > source_w || y > source_h)
	return;
	uint8_t *p = (uint8_t *)source_graph + y * source_pitch + x * 2;
	*(uint16_t *)p = pixel;
}

void line(short x1, short y1, short x2, short y2, uint32_t color) 
{
    signed char ix;
    signed char iy;
 
    // if x1 == x2 or y1 == y2, then it does not matter what we set here
    unsigned short delta_x = (x2 > x1?(ix = 1, x2 - x1):(ix = -1, x1 - x2)) << 1;
    unsigned short delta_y = (y2 > y1?(iy = 1, y2 - y1):(iy = -1, y1 - y2)) << 1;
 
	setPixel(x1,y1,UINT16_MAX);
    if (delta_x >= delta_y) {
        int error = delta_y - (delta_x >> 1);        // error may go below zero
        while (x1 != x2) {
            if (error >= 0) {
                if (error || (ix > 0)) {
                    y1 += iy;
                    error -= delta_x;
                }                           // else do nothing
         }                              // else do nothing
            x1 += ix;
            error += delta_y;
            setPixel(x1,y1,color);
        }
    } else {
        int error = delta_x - (delta_y >> 1);      // error may go below zero
        while (y1 != y2) {
            if (error >= 0) {
                if (error || (iy > 0)) {
                    x1 += ix;
                    error -= delta_y;
                }                           // else do nothing
            }                              // else do nothing
            y1 += iy;
            error += delta_x;  
            setPixel(x1,y1, color);
        }
    }
}


// remove_ext: removes the "extension" from a file spec.
//   mystr is the string to process.
//   dot is the extension separator.
//   sep is the path separator (0 means to ignore).
// Returns an allocated string identical to the original but
//   with the extension removed. It must be freed when you're
//   finished with it.
// If you pass in NULL or the new string can't be allocated,
//   it returns NULL.

char *remove_ext (char* mystr, char dot, char sep) 
{
    char *retstr, *lastdot, *lastsep;
    // Error checks and allocate string.
    if (mystr == NULL)
        return NULL;
    if ((retstr = malloc (strlen (mystr) + 1)) == NULL)
        return NULL;

    // Make a copy and find the relevant characters.
    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

    // If it has an extension separator.
    if (lastdot != NULL) {
        // and it's before the extenstion separator.
        if (lastsep != NULL) {
            if (lastsep < lastdot) {
                // then remove it.
                *lastdot = '\0';
            }
        } else {
            // Has extension separator with no path separator.
            *lastdot = '\0';
        }
    }

    // Return the modified string.
    return retstr;
}

void Init_Video(char* overlay_path)
{
	extern char overlay_path_home[256];
	char new_overlay[256];
	SDL_Surface *overlay_original, *tmp;
	
	SDL_Init(SDL_INIT_VIDEO);
	
	sdl_screen = SDL_SetVideoMode(0, 0, 16, SDL_HWSURFACE
	#ifdef SDL_TRIPLEBUF
	| SDL_TRIPLEBUF
	#endif
	);
	
	/* Screen geometry wrong */
	if (sdl_screen->w == 320 && sdl_screen->h == 480)
	{
		sdl_screen = SDL_SetVideoMode(400, 480, 16, SDL_HWSURFACE
		#ifdef SDL_TRIPLEBUF
		| SDL_TRIPLEBUF
		#endif
		);
	}
	
	screenx = sdl_screen->w;
	screeny = sdl_screen->h;
	
	backbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 16, 0,0,0,0);

	Set_resolution_vector();
	
	/* We extract the directory and filename (without the file extension) from the file path. Then we simply add the .png extension. */
	snprintf(new_overlay, sizeof(new_overlay), "%s/%s.png", dirname(overlay_path), remove_ext (basename(overlay_path), '.', '/'));
	overlay_original = IMG_Load(new_overlay);
	if (!overlay_original)
	{
		snprintf(new_overlay, sizeof(new_overlay), "%s/%s.png", overlay_path_home, remove_ext (basename(overlay_path), '.', '/'));
		overlay_original = IMG_Load(new_overlay);
	}
	
	if (overlay_original)
	{
		if(overlay)
			SDL_FreeSurface(overlay);
		double overlay_scale = ((double)ALG_MAX_X / (double)scl_factor) / (double)overlay_original->w;
		tmp = zoomSurface(overlay_original, overlay_scale, overlay_scale, 0);
		overlay = SDL_DisplayFormat(tmp); 
		SDL_FreeSurface(tmp);
		SDL_FreeSurface(overlay_original);
		SDL_SetAlpha(overlay, SDL_SRCALPHA, 128);
	}
}


void Set_Video_Menu()
{
	sdl_screen = SDL_SetVideoMode(320, 240, 16, SDL_HWSURFACE
	#ifdef SDL_TRIPLEBUF
	| SDL_TRIPLEBUF
	#endif
	);
}

void Set_Video_InGame()
{
	sdl_screen = SDL_SetVideoMode(screenx, screeny, 16, SDL_HWSURFACE
	#ifdef SDL_TRIPLEBUF
	| SDL_TRIPLEBUF
	#endif
	);
	
	Set_resolution_vector();

	SDL_FillRect(sdl_screen, NULL, 0);
	SDL_FillRect(backbuffer, NULL, 0);
}

void Close_Video()
{
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	if (backbuffer) SDL_FreeSurface(backbuffer);
	if (overlay) SDL_FreeSurface(overlay);
	SDL_Quit();
}

void Update_Video_Menu()
{
	SDL_Flip(sdl_screen);
}

void Update_Video_Ingame()
{
	uint_fast32_t v;
	
	Set_resolution_vector();
	
	for(v = 0; v < vector_draw_cnt; v++)
	{
		uint8_t c = vectors_draw[v].color * 256 / VECTREX_COLORS;
		aalineRGBA(sdl_screen,
				offx + vectors_draw[v].x0 / scl_factor,
				offy + vectors_draw[v].y0 / scl_factor,
				offx + vectors_draw[v].x1 / scl_factor,
				offy + vectors_draw[v].y1 / scl_factor,
				c, c, c, 0xff);
	}
	
	if(overlay)
	{
		SDL_Rect dest_rect = {offx, offy, 0, 0};
		SDL_BlitSurface(overlay, NULL, sdl_screen, &dest_rect);
	}
	
	SDL_Flip(sdl_screen);
}

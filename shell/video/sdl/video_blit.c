/* Frontend for Vecx
 * MIT License, Copyright 2019 Gameblabla
 * */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <sys/time.h>
#include <sys/types.h>

#include "e6809.h"
#include "e8910.h"
#include "vecx.h"

#include "video_blit.h"
#include "config.h"

SDL_Surface *sdl_screen, *backbuffer, *internal_buffer;

uint32_t width_of_surface;
uint16_t* Draw_to_Virtual_Screen;

SDL_Surface *overlay;

static long scl_factor;
static long offx;
static long offy;

static long sclx, scly;

static uint16_t* restrict source_graph;
static uint32_t source_w, source_h, source_pitch;

void Set_resolution_vector()
{
	switch(option.fullscreen)
	{
		case 0:
			source_graph = sdl_screen->pixels;
			source_w = sdl_screen->w;
			source_h = sdl_screen->h;
			source_pitch = sdl_screen->pitch;
			
			sclx = ALG_MAX_X / 240;
			scly = ALG_MAX_Y / 320;

			scl_factor = sclx > scly ? sclx : scly;
			
			offx = 32;
			offy = -29;
			SDL_FillRect(sdl_screen, NULL, 0);
		break;
		case 1:
		case 2:
			source_graph = internal_buffer->pixels;
			source_w = internal_buffer->w;
			source_h = internal_buffer->h;
			source_pitch = internal_buffer->pitch;
		
			sclx = ALG_MAX_X / 320;
			scly = ALG_MAX_Y / 380;

			scl_factor = sclx > scly ? sclx : scly;
			
			offx = -36;
			offy = -30;
			SDL_FillRect(internal_buffer, NULL, 0);
		break;
	}
}

void setPixel(uint_fast32_t x, uint_fast32_t y, Uint32 pixel)
{
	if (x > source_w || y > source_h)
	return;
	uint8_t *p = (uint8_t *)source_graph + y * source_pitch + x * 2;
	*(uint16_t *)p = pixel;
}

void line(short x1, short y1, short x2, short y2) 
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
            setPixel(x1,y1,UINT16_MAX);
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
            setPixel(x1,y1, UINT16_MAX);
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
	char new_overlay[256];
	long screenx, screeny;
	SDL_Surface *image, *tmp, *tmp2;
	
	sdl_screen = SDL_SetVideoMode(HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, 16, SDL_HWSURFACE
	#ifdef SDL_TRIPLEBUF
	| SDL_TRIPLEBUF
	#endif
	);
	
	backbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 16, 0,0,0,0);
	internal_buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 240, 320, 16, 0,0,0,0);
	
	screenx = 240;
	screeny = 240;
	
	/*
	sclx = ALG_MAX_X / sdl_screen->w;
	scly = ALG_MAX_Y / sdl_screen->h;

	scl_factor = sclx > scly ? sclx : scly;

	offx = (screenx - ALG_MAX_X / scl_factor) / 2;
	offy = (screeny - ALG_MAX_Y / scl_factor) / 2;
	*/
	
	Set_resolution_vector();
	
	/* We extract the directory and filename (without the file extension) from the file path. Then we simply add the .png extension. */
	snprintf(new_overlay, sizeof(new_overlay), "%s/%s.png", dirname(overlay_path), remove_ext (basename(overlay_path), '.', '/'));
	image = IMG_Load(new_overlay);
	
	if (image)
	{
		tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, 240, 240, 16, 0, 0, 0, 0);
		tmp2 = SDL_DisplayFormat(image);
		
		SDL_SoftStretch(tmp2, NULL, tmp, NULL);
		
		overlay = SDL_DisplayFormat(tmp);
		
		SDL_FreeSurface(tmp);
		SDL_FreeSurface(tmp2);
		
		SDL_SetAlpha(overlay, SDL_SRCALPHA, 128);
	}
}


void Set_Video_Menu()
{
}

void Set_Video_InGame()
{
	SDL_FillRect(sdl_screen, NULL, 0);
	SDL_FillRect(backbuffer, NULL, 0);
	SDL_FillRect(internal_buffer, NULL, 0);
}

void Close_Video()
{
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	if (backbuffer) SDL_FreeSurface(backbuffer);
	if (overlay) SDL_FreeSurface(overlay);
	if (internal_buffer) SDL_FreeSurface(internal_buffer);
	SDL_Quit();
}

void Update_Video_Menu()
{
	SDL_Flip(sdl_screen);
}

static void rotate_90_ccw(uint16_t* restrict dst, uint16_t* restrict src)
{
    int32_t h = 240, w = 320;
    src += w * h - 1;
    for (int32_t col = w - 1; col >= 0; --col)
    {
        uint16_t *outcol = dst + col;
        for(int32_t row = 0; row < h; ++row, outcol += w)
        {
            *outcol = *src--;
		}
    }
}

static void rotate_90_cw(uint16_t* restrict dst, uint16_t* restrict src)
{
	int32_t h = 240, w = 320;
    for (int32_t x = 0; x < h; x++) {
        for (int32_t y = w - 1; y >=0; y--) {
            *dst++ = src[y * h + x];
        }
    }
}

void Update_Video_Ingame()
{
	uint_fast32_t v;
	
	Set_resolution_vector();
	
	for(v = 0; v < vector_draw_cnt; v++)
	{
		line(
				(vectors_draw[v].x0 / scl_factor) + offx, 
				(vectors_draw[v].y0 / scl_factor) + offy, 
				(vectors_draw[v].x1 / scl_factor) + offx, 
				(vectors_draw[v].y1 / scl_factor) + offy
			);
	}
	
	switch(option.fullscreen)
	{
		case 0:
			if (overlay)
			{
				SDL_Rect dest_rect = {32, 0, 0, 0};
				SDL_BlitSurface(overlay, NULL, sdl_screen, &dest_rect);
			}
		break;
		case 1:
			rotate_90_ccw((uint16_t* restrict)sdl_screen->pixels, (uint16_t* restrict)internal_buffer->pixels);
		break;
		case 2:
			rotate_90_cw((uint16_t* restrict)sdl_screen->pixels, (uint16_t* restrict)internal_buffer->pixels);
		break;
	}
	
	SDL_Flip(sdl_screen);
}

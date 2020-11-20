#ifndef VIDEO_BLIT_H
#define VIDEO_BLIT_H

#include <SDL/SDL.h>

extern SDL_Surface *sdl_screen, *backbuffer;

extern uint32_t width_of_surface;
extern uint16_t* Draw_to_Virtual_Screen;

void Init_Video();
void Set_Video_Menu();
void Set_Video_InGame();
void Close_Video();
void Update_Video_Menu();
void Update_Video_Ingame();

#endif

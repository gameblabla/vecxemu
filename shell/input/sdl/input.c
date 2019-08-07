#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "menu.h"
#include "vecx.h"
#include "config.h"

void update_input(void)
{
	SDL_Event event;
	uint8_t* keys;
	
	keys = SDL_GetKeyState(NULL);

	while (SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_KEYDOWN:
			switch(event.key.keysym.sym)
			{
				case SDLK_END:
				case SDLK_RCTRL:
				case SDLK_ESCAPE:
					emulator_state = 1;
				break;
				default:
					if (event.key.keysym.sym == option.config_buttons[0])
						alg.jch1 = 0xff;
					else if (event.key.keysym.sym == option.config_buttons[1])
						alg.jch1 = 0x00;
					else if (event.key.keysym.sym == option.config_buttons[2])
						alg.jch0 = 0x00;
					else if (event.key.keysym.sym == option.config_buttons[3])
						alg.jch0 = 0xff;
					else if (event.key.keysym.sym == option.config_buttons[4])
						snd_regs[14] &= ~0x01;
					else if (event.key.keysym.sym == option.config_buttons[5])
						snd_regs[14] &= ~0x02;
					else if (event.key.keysym.sym == option.config_buttons[6])
						snd_regs[14] &= ~0x04;
					else if (event.key.keysym.sym == option.config_buttons[7])
						snd_regs[14] &= ~0x08;
				break;
			}
			break;
			case SDL_KEYUP:
				default:
					if (event.key.keysym.sym == option.config_buttons[0])
						alg.jch1 = 0x80;
					else if (event.key.keysym.sym == option.config_buttons[1])
						alg.jch1 = 0x80;
					else if (event.key.keysym.sym == option.config_buttons[2])
						alg.jch0 = 0x80;
					else if (event.key.keysym.sym == option.config_buttons[3])
						alg.jch0 = 0x80;
					else if (event.key.keysym.sym == option.config_buttons[4])
						snd_regs[14] |= 0x01;
					else if (event.key.keysym.sym == option.config_buttons[5])
						snd_regs[14] |= 0x02;
					else if (event.key.keysym.sym == option.config_buttons[6])
						snd_regs[14] |= 0x04;
					else if (event.key.keysym.sym == option.config_buttons[7])
						snd_regs[14] |= 0x08;
				break;
			break;
		}
	}
}


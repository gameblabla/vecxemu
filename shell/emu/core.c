/* Frontend for Vecx
 * MIT License, Copyright 2019 Gameblabla
 * */
#include <libgen.h>
#include <stdint.h>
#include <sys/time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "e6809.h"
#include "e8910.h"
#include "vecx.h"

#include "video_blit.h"
#include "sound_output.h"
#include "input.h"
#include "menu.h"

#define EMU_TIMER 20 /* the emulators heart beats at 20 milliseconds */

char GameName_emu[512];
char BIOSRom_path[512];
uint_fast8_t emulator_state = 0;
uint_fast8_t done = 0;
Uint32 next_time;

void SaveState(const char* path, uint_fast8_t load)
{
	FILE* fp;
	if (load)
		fp = fopen(path, "rb");
	else
		fp = fopen(path, "wb");
		
	if (!fp) return;
	
	e6809_state(load, fp);
	e8910_state(load, fp);
	sys_state(load, fp);
	
	fclose(fp);
}

static uint32_t Load_BIOS(char* romfilename)
{
	FILE *f;
	long sz;
	
	f = fopen(romfilename, "rb");
	if (!f)
	{
		perror(romfilename);
		return 0;
	}
	

	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	
	if (sz > sizeof(rom))
	{
		printf("Internal ROM bigger than internally allocated rom size. (8K)\n");
		return 0;
	}
	
	fseek(f, 0L, SEEK_SET);
	
	memset(rom, 0, sizeof(rom));
	fread(rom, sizeof(uint8_t), sz, f);
	fclose(f);
	
	return 1;
}

static uint32_t Load_Game(char* cartfilename)
{
	FILE *f;
	long sz;
	
	f = fopen(cartfilename, "rb");
	if (!f)
	{
		perror(cartfilename);
		return 0;
	}
		
	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	
	if (sz > sizeof(cart))
	{
		printf("Cartridge ROM bigger than internally allocated size. (48K)\n");
		return 0;
	}
	
	fseek(f, 0L, SEEK_SET);
	
	memset(cart, 0, sizeof(cart));
		
	fread(cart, sizeof(uint8_t), sizeof(cart), f);
	fclose(f);
	
	return 1;
}

int main(int argc, char* argv[])
{
	uint32_t res;
	
    printf("Starting VecxEMU\n");

	Init_Configuration();

	snprintf(BIOSRom_path, sizeof(BIOSRom_path), "%s/%s", home_path, "rom.dat");
	res = Load_BIOS(BIOSRom_path);
	
	/* If we find a Cartridge being loaded from command line then load that with its overlay. */
	if (argc > 1)
	{
		snprintf(GameName_emu, sizeof(GameName_emu), "%s", basename(argv[1]));
		Load_Game(argv[1]);
		Init_Video(argv[1]);
	}
	/* If not then use the overlay for the internal ROM. */
	else
	{
		snprintf(GameName_emu, sizeof(GameName_emu), "%s", basename(BIOSRom_path));
		printf("No cart loaded : Using internal ROM.\n");
		if (!res)
		{
			printf("But there's no internal ROM ! Exit.\n");
			return 0;
		}
		Init_Video(BIOSRom_path);
	}
	
	Audio_Init();

	next_time = SDL_GetTicks() + EMU_TIMER;
	vecx_reset();
	
	while(!done)
	{
		switch(emulator_state)
		{
			case 0:
				vecx_emu((VECTREX_MHZ / 1000) * EMU_TIMER);
				update_input();
				{
					Uint32 now = SDL_GetTicks();
					if(now < next_time)
						SDL_Delay(next_time - now);
					else
						next_time = now;
					next_time += EMU_TIMER;
				}
			break;
			case 1:
				Menu();
			break;
		}

	}
	
	Audio_Close();
	Close_Video();
	
	return 0;
}

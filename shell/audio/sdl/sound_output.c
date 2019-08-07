#include <sys/ioctl.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <SDL/SDL.h>

#include "e8910.h"

#include "sound_output.h"

uint32_t Audio_Init()
{
	e8910_init_sound();
	return 0;
}

void Audio_Close()
{
	e8910_done_sound();
}

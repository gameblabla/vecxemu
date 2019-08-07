PRGNAME     = vecx.elf
CC          = gcc

# change compilation / linking flag options

INCLUDES	= -Ishell/input/sdl -Icore -Icore/sound -Ishell/menu -Ishell/video/sdl -Ishell/audio -Ishell/scalers -Ishell/headers -Ishell/fonts
DEFINES		= -DLSB_FIRST -DINLINE="inline" -DWANT_16BPP -DFRONTEND_SUPPORTS_RGB565
DEFINES		+= -DFRAMESKIP

CFLAGS		= -Ofast -march=native -flto -std=gnu99 $(INCLUDES) $(DEFINES)
LDFLAGS     = -lSDL -lSDL_image -lm -lasound -flto -s

# Files to be compiled
SRCDIR 		= ./core
SRCDIR 		+= ./shell/video/sdl ./shell/audio/sdl ./shell/input/sdl ./shell/menu ./shell/emu ./shell/fonts ./shell/scalers
VPATH		= $(SRCDIR)
SRC_C		= $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
OBJ_C		= $(notdir $(patsubst %.c, %.o, $(SRC_C)))
OBJS		= $(OBJ_C)

# Rules to make executable
$(PRGNAME): $(OBJS)  
	$(CC) $(CFLAGS) -o $(PRGNAME) $^ $(LDFLAGS)
	
$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PRGNAME)$(EXESUFFIX) *.o

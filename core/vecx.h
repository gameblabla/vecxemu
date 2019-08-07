#ifndef __VECX_H
#define __VECX_H

/* GPLv3 License - See LICENSE.md for more information. */

/* Gameblabla August 7th 2019 :
 * - Increase cart variable lengh to allow 48k roms
 * - Allow saving of VIA, analog variables. It's quite big though. Useful for save state
 * */
 

/* Was set to 0x8000 before but apparently 48k carts are supported too. */
#define MAX_CART_SIZE 0xC000

enum {
	VECTREX_MHZ		= 1500000, /* speed of the vectrex being emulated */
	VECTREX_COLORS  = 128,     /* number of possible colors ... grayscale */

	ALG_MAX_X		= 33000,
	ALG_MAX_Y		= 41000
};

typedef struct vector_type {
	long x0, y0; /* start coordinate */
	long x1, y1; /* end coordinate */

	/* color [0, VECTREX_COLORS - 1], if color = VECTREX_COLORS, then this is
	 * an invalid entry and must be ignored.
	 */
	unsigned char color;
} vector_t;

struct analog_chips 
{
	unsigned rsh;  /* zero ref sample and hold */
	unsigned xsh;  /* x sample and hold */
	unsigned ysh;  /* y sample and hold */
	unsigned zsh;  /* z sample and hold */
	unsigned jch0;		  /* joystick direction channel 0 */
	unsigned jch1;		  /* joystick direction channel 1 */
	unsigned jch2;		  /* joystick direction channel 2 */
	unsigned jch3;		  /* joystick direction channel 3 */
	unsigned jsh;  /* joystick sample and hold */
	unsigned compare;
	long dx;     /* delta x */
	long dy;     /* delta y */
	long curr_x; /* current x position */
	long curr_y; /* current y position */
	unsigned vectoring; /* are we drawing a vector right now? */
	long vector_x0;
	long vector_y0;
	long vector_x1;
	long vector_y1;
	long vector_dx;
	long vector_dy;
	unsigned char vector_color;
};

extern unsigned char rom[8192];
extern unsigned char cart[32768+16384];

extern unsigned snd_regs[16];

extern struct analog_chips alg;

extern long vector_draw_cnt;
extern long vector_erse_cnt;
extern vector_t *vectors_draw;
extern vector_t *vectors_erse;

extern void vecx_reset (void);
extern void vecx_emu (long cycles);

extern void sys_state(uint_fast8_t load, FILE* fp);

#endif

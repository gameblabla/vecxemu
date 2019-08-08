/* GPLv3 License - See LICENSE.md for more information. */

/* Gameblabla August 7th 2019 :
 * - Allow saving of VIA, analog variables. It's quite big though. Useful for save state
 * */
 
#include <stdio.h>
#include "e6809.h"
#include "vecx.h"
#include "video_blit.h"
#include "e8910.h"

#define einline __inline

unsigned char rom[8192];
unsigned char cart[65536];
static unsigned char ram[1024];

/* BankSwitching code from VecXGL fork */

#define BS_0 0
#define BS_1 1
#define BS_2 2
#define BS_3 3
#define BS_4 4
#define BS_5 5

unsigned newbankswitchOffset = 0;
unsigned bankswitchOffset = 0;
unsigned char big = 0;

unsigned char get_cart(unsigned pos)
{
	return cart[(pos+bankswitchOffset)%65536];
}

void set_cart(unsigned pos, unsigned char data)
{
	// 64k carts should have data at this offset
	if (pos == 32768 && data != 0)
	{
		big = 1;
	} 
	cart[(pos)%65536] = data; 
}

unsigned bankswitchstate = BS_0;


/* the sound chip registers */

unsigned snd_regs[16];
static unsigned snd_select;

/* the via 6522 registers */

struct VIA_CHIP 
{
	 unsigned ora;
	 unsigned orb;
	 unsigned ddra;
	 unsigned ddrb;
	 unsigned t1on;  /* is timer 1 on? */
	 unsigned t1int; /* are timer 1 interrupts allowed? */
	 unsigned t1c;
	 unsigned t1ll;
	 unsigned t1lh;
	 unsigned t1pb7; /* timer 1 controlled version of pb7 */
	 unsigned t2on;  /* is timer 2 on? */
	 unsigned t2int; /* are timer 2 interrupts allowed? */
	 unsigned t2c;
	 unsigned t2ll;
	 unsigned sr;
	 unsigned srb;   /* number of bits shifted so far */
	 unsigned src;   /* shift counter */
	 unsigned srclk;
	 unsigned acr;
	 unsigned pcr;
	 unsigned ifr;
	 unsigned ier;
	 unsigned ca2;
	 unsigned cb2h;  /* basic handshake version of cb2 */
	 unsigned cb2s;  /* version of cb2 controlled by the shift register */
} via;


/* analog devices */

struct analog_chips alg;


enum {
	VECTREX_PDECAY	= 30,      /* phosphor decay rate */

	/* number of 6809 cycles before a frame redraw */

	FCYCLES_INIT    = VECTREX_MHZ / VECTREX_PDECAY,

	/* max number of possible vectors that maybe on the screen at one time.
	 * one only needs VECTREX_MHZ / VECTREX_PDECAY but we need to also store
	 * deleted vectors in a single table
	 */

	VECTOR_CNT		= VECTREX_MHZ / VECTREX_PDECAY,

	VECTOR_HASH     = 65521
};

long vector_draw_cnt;
long vector_erse_cnt;
static vector_t vectors_set[2 * VECTOR_CNT];
vector_t *vectors_draw;
vector_t *vectors_erse;

static long vector_hash[VECTOR_HASH];

static long fcycles;

void sys_state(uint_fast8_t load, FILE* fp)
{	
	if (load == 1)
	{
		fread(&fcycles, sizeof(uint8_t), sizeof(fcycles), fp);
		
		fread(&alg, sizeof(uint8_t), sizeof(alg), fp);
		fread(&via, sizeof(uint8_t), sizeof(via), fp);
		
		fread(&ram, sizeof(uint8_t), sizeof(ram), fp);
		fread(&snd_regs, sizeof(uint8_t), sizeof(snd_regs), fp);
		
		/* Reset vectors_set to 0 */
		memset(vectors_set, 0, (2 * VECTOR_CNT) * sizeof(vector_t));
	}
	else
	{
		fwrite(&fcycles, sizeof(uint8_t), sizeof(fcycles), fp);

		fwrite(&alg, sizeof(uint8_t), sizeof(alg), fp);
		fwrite(&via, sizeof(uint8_t), sizeof(via), fp);
		
		fwrite(&ram, sizeof(uint8_t), sizeof(ram), fp);
		fwrite(&snd_regs, sizeof(uint8_t), sizeof(snd_regs), fp);
	}
}


/* update the snd chips internal registers when via.ora/via.orb changes */

static einline void snd_update (void)
{
	switch (via.orb & 0x18) {
	case 0x00:
		/* the sound chip is disabled */
		break;
	case 0x08:
		/* the sound chip is sending data */
		break;
	case 0x10:
		/* the sound chip is recieving data */

		if (snd_select != 14) {
			snd_regs[snd_select] = via.ora;
			e8910_write(snd_select, via.ora);
		}

		break;
	case 0x18:
		/* the sound chip is latching an address */

		if ((via.ora & 0xf0) == 0x00) {
			snd_select = via.ora & 0x0f;
		}

		break;
	}
}

/* update the various analog values when orb is written. */

static einline void alg_update (void)
{
	switch (via.orb & 0x06) {
	case 0x00:
		alg.jsh = alg.jch0;

		if ((via.orb & 0x01) == 0x00) {
			/* demultiplexor is on */
			alg.ysh = alg.xsh;
		}

		break;
	case 0x02:
		alg.jsh = alg.jch1;

		if ((via.orb & 0x01) == 0x00) {
			/* demultiplexor is on */
			alg.rsh = alg.xsh;
		}

		break;
	case 0x04:
		alg.jsh = alg.jch2;

		if ((via.orb & 0x01) == 0x00) {
			/* demultiplexor is on */

			if (alg.xsh > 0x80) {
				alg.zsh = alg.xsh - 0x80;
			} else {
				alg.zsh = 0;
			}
		}

		break;
	case 0x06:
		/* sound output line */
		alg.jsh = alg.jch3;
		break;
	}

	/* compare the current joystick direction with a reference */

	if (alg.jsh > alg.xsh) {
		alg.compare = 0x20;
	} else {
		alg.compare = 0;
	}

	/* compute the new "deltas" */

	alg.dx = (long) alg.xsh - (long) alg.rsh;
	alg.dy = (long) alg.rsh - (long) alg.ysh;
}

/* update IRQ and bit-7 of the ifr register after making an adjustment to
 * ifr.
 */

static einline void int_update (void)
{
	if ((via.ifr & 0x7f) & (via.ier & 0x7f)) {
		via.ifr |= 0x80;
	} else {
		via.ifr &= 0x7f;
	}
}

unsigned char read8 (unsigned address)
{
	unsigned char data;

	if ((address & 0xe000) == 0xe000) {
		/* rom */
		data = rom[address & 0x1fff];
	} else if ((address & 0xe000) == 0xc000) {
		if (address & 0x800) {
			/* ram */

			data = ram[address & 0x3ff];
		} else if (address & 0x1000) {
			/* io */

			switch (address & 0xf) {
			case 0x0:
				/* compare signal is an input so the value does not come from
				 * via.orb.
				 */

				if (via.acr & 0x80) {
					/* timer 1 has control of bit 7 */

					data = (unsigned char) ((via.orb & 0x5f) | via.t1pb7 | alg.compare);
				} else {
					/* bit 7 is being driven by via.orb */

					data = (unsigned char) ((via.orb & 0xdf) | alg.compare);
				}

				break;
			case 0x1:
				/* register 1 also performs handshakes if necessary */

				if ((via.pcr & 0x0e) == 0x08) {
					/* if ca2 is in pulse mode or handshake mode, then it
					 * goes low whenever ira is read.
					 */

					via.ca2 = 0;
				}

				/* fall through */

			case 0xf:
				if ((via.orb & 0x18) == 0x08) {
					/* the snd chip is driving port a */

					data = (unsigned char) snd_regs[snd_select];
				} else {
					data = (unsigned char) via.ora;
				}

				break;
			case 0x2:
				data = (unsigned char) via.ddrb;
				break;
			case 0x3:
				data = (unsigned char) via.ddra;
				break;
			case 0x4:
				/* T1 low order counter */

				data = (unsigned char) via.t1c;
				via.ifr &= 0xbf; /* remove timer 1 interrupt flag */

				via.t1on = 0; /* timer 1 is stopped */
				via.t1int = 0;
				via.t1pb7 = 0x80;

				int_update ();

				break;
			case 0x5:
				/* T1 high order counter */

				data = (unsigned char) (via.t1c >> 8);

				break;
			case 0x6:
				/* T1 low order latch */

				data = (unsigned char) via.t1ll;
				break;
			case 0x7:
				/* T1 high order latch */

				data = (unsigned char) via.t1lh;
				break;
			case 0x8:
				/* T2 low order counter */

				data = (unsigned char) via.t2c;
				via.ifr &= 0xdf; /* remove timer 2 interrupt flag */

				via.t2on = 0; /* timer 2 is stopped */
				via.t2int = 0;

				int_update ();

				break;
			case 0x9:
				/* T2 high order counter */

				data = (unsigned char) (via.t2c >> 8);
				break;
			case 0xa:
				data = (unsigned char) via.sr;
				via.ifr &= 0xfb; /* remove shift register interrupt flag */
				via.srb = 0;
				via.srclk = 1;

				int_update ();

				break;
			case 0xb:
				data = (unsigned char) via.acr;
				break;
			case 0xc:
				data = (unsigned char) via.pcr;
				break;
			case 0xd:
				/* interrupt flag register */

				data = (unsigned char) via.ifr;
				break;
			case 0xe:
				/* interrupt enable register */

				data = (unsigned char) (via.ier | 0x80);
				break;
			}
		}
	} else if (address < MAX_CART_SIZE) {
		/* cartridge */

		data = get_cart(address);
	} else {
		data = 0xff;
	}

	return data;
}

void write8 (unsigned address, unsigned char data)
{
	if ((address & 0xe000) == 0xe000) {
		/* rom */
	} else if ((address & 0xe000) == 0xc000) {
		/* it is possible for both ram and io to be written at the same! */

		if (address & 0x800) {
			ram[address & 0x3ff] = data;
		}

		if (address & 0x1000) {
			switch (address & 0xf) {
			case 0x0:
				if (bankswitchstate == BS_2)
				{
					if (data == 1)
						bankswitchstate = BS_3;
					else
						bankswitchstate = BS_0;
				}
				else 
				{
					bankswitchstate = BS_0;
				}
				via.orb = data;

				snd_update ();

				alg_update ();

				if ((via.pcr & 0xe0) == 0x80) {
					/* if cb2 is in pulse mode or handshake mode, then it
					 * goes low whenever orb is written.
					 */

					via.cb2h = 0;
				}

				break;
			case 0x1:
				/* register 1 also performs handshakes if necessary */
				
				if (bankswitchstate == BS_3)
				{
					if (data == 0)
						bankswitchstate = BS_4;
					else
						bankswitchstate = BS_0;
				} 
				else 
				{
					bankswitchstate = BS_0;
				}
				
				if ((via.pcr & 0x0e) == 0x08) {
					/* if ca2 is in pulse mode or handshake mode, then it
					 * goes low whenever ora is written.
					 */

					via.ca2 = 0;
				}

				/* fall through */

			case 0xf:
				via.ora = data;

				snd_update ();

				/* output of port a feeds directly into the dac which then
				 * feeds the x axis sample and hold.
				 */

				alg.xsh = data ^ 0x80;

				alg_update ();

				break;
			case 0x2:
				via.ddrb = data;
				
				bankswitchstate = BS_1;
				/* Don't use bankswitching code for non-expanded games */
				if (!big || (data & 0x40))
					newbankswitchOffset = 0;
				else
					newbankswitchOffset = 32768;
					
				break;
			case 0x3:
				via.ddra = data;
				break;
			case 0x4:
				if (bankswitchstate == BS_5)
				{
					bankswitchOffset = newbankswitchOffset;
					bankswitchstate = BS_0;
				}
				/* T1 low order counter */
				via.t1ll = data;

				break;
			case 0x5:
				/* T1 high order counter */

				via.t1lh = data;
				via.t1c = (via.t1lh << 8) | via.t1ll;
				via.ifr &= 0xbf; /* remove timer 1 interrupt flag */

				via.t1on = 1; /* timer 1 starts running */
				via.t1int = 1;
				via.t1pb7 = 0;

				int_update ();

				break;
			case 0x6:
				/* T1 low order latch */

				via.t1ll = data;
				break;
			case 0x7:
				/* T1 high order latch */

				via.t1lh = data;
				break;
			case 0x8:
				/* T2 low order latch */

				via.t2ll = data;
				break;
			case 0x9:
				/* T2 high order latch/counter */

				via.t2c = (data << 8) | via.t2ll;
				via.ifr &= 0xdf;

				via.t2on = 1; /* timer 2 starts running */
				via.t2int = 1;

				int_update ();

				break;
			case 0xa:
				via.sr = data;
				via.ifr &= 0xfb; /* remove shift register interrupt flag */
				via.srb = 0;
				via.srclk = 1;

				int_update ();

				break;
			case 0xb:
				if (bankswitchstate == BS_4)
				{
					if (data == 0x98)
						bankswitchstate = BS_5;
					else
						bankswitchstate = BS_0;
				}
				else 
				{
					bankswitchstate = BS_0;
				}
				via.acr = data;
				break;
			case 0xc:
				via.pcr = data;


				if ((via.pcr & 0x0e) == 0x0c) {
					/* ca2 is outputting low */

					via.ca2 = 0;
				} else {
					/* ca2 is disabled or in pulse mode or is
					 * outputting high.
					 */

					via.ca2 = 1;
				}

				if ((via.pcr & 0xe0) == 0xc0) {
					/* cb2 is outputting low */

					via.cb2h = 0;
				} else {
					/* cb2 is disabled or is in pulse mode or is
					 * outputting high.
					 */

					via.cb2h = 1;
				}

				break;
			case 0xd:
				/* interrupt flag register */

				via.ifr &= ~(data & 0x7f);
				int_update ();

				break;
			case 0xe:
				/* interrupt enable register */

				if (data & 0x80) {
					via.ier |= data & 0x7f;
				} else {
					via.ier &= ~(data & 0x7f);
				}

				int_update ();

				break;
			}
		}
	} else if (address < MAX_CART_SIZE) {
		/* cartridge */
	}
}

void vecx_reset (void)
{
	unsigned r;

	/* ram */

	for (r = 0; r < sizeof(ram); r++) {
		ram[r] = r & 0xff;
	}

	for (r = 0; r < 16; r++) {
		snd_regs[r] = 0;
		e8910_write(r, 0);
	}

	/* input buttons */

	snd_regs[14] = 0xff;
	e8910_write(14, 0xff);

	snd_select = 0;

	via.ora = 0;
	via.orb = 0;
	via.ddra = 0;
	via.ddrb = 0;
	via.t1on = 0;
	via.t1int = 0;
	via.t1c = 0;
	via.t1ll = 0;
	via.t1lh = 0;
	via.t1pb7 = 0x80;
	via.t2on = 0;
	via.t2int = 0;
	via.t2c = 0;
	via.t2ll = 0;
	via.sr = 0;
	via.srb = 8;
	via.src = 0;
	via.srclk = 0;
	via.acr = 0;
	via.pcr = 0;
	via.ifr = 0;
	via.ier = 0;
	via.ca2 = 1;
	via.cb2h = 1;
	via.cb2s = 0;

	alg.rsh = 128;
	alg.xsh = 128;
	alg.ysh = 128;
	alg.zsh = 0;
	alg.jch0 = 128;
	alg.jch1 = 128;
	alg.jch2 = 128;
	alg.jch3 = 128;
	alg.jsh = 128;

	alg.compare = 0; /* check this */

	alg.dx = 0;
	alg.dy = 0;
	alg.curr_x = ALG_MAX_X / 2;
	alg.curr_y = ALG_MAX_Y / 2;

	alg.vectoring = 0;

	vector_draw_cnt = 0;
	vector_erse_cnt = 0;
	vectors_draw = vectors_set;
	vectors_erse = vectors_set + VECTOR_CNT;

	fcycles = FCYCLES_INIT;

	e6809_read8 = read8;
	e6809_write8 = write8;

	e6809_reset ();
}

/* perform a single cycle worth of via emulation.
 * via_sstep0 is the first postion of the emulation.
 */

static einline void via_sstep0 (void)
{
	unsigned t2shift;

	if (via.t1on) {
		via.t1c--;

		if ((via.t1c & 0xffff) == 0xffff) {
			/* counter just rolled over */

			if (via.acr & 0x40) {
				/* continuous interrupt mode */

				via.ifr |= 0x40;
				int_update ();
				via.t1pb7 = 0x80 - via.t1pb7;

				/* reload counter */

				via.t1c = (via.t1lh << 8) | via.t1ll;
			} else {
				/* one shot mode */

				if (via.t1int) {
					via.ifr |= 0x40;
					int_update ();
					via.t1pb7 = 0x80;
					via.t1int = 0;
				}
			}
		}
	}

	if (via.t2on && (via.acr & 0x20) == 0x00) {
		via.t2c--;

		if ((via.t2c & 0xffff) == 0xffff) {
			/* one shot mode */

			if (via.t2int) {
				via.ifr |= 0x20;
				int_update ();
				via.t2int = 0;
			}
		}
	}

	/* shift counter */

	via.src--;

	if ((via.src & 0xff) == 0xff) {
		via.src = via.t2ll;

		if (via.srclk) {
			t2shift = 1;
			via.srclk = 0;
		} else {
			t2shift = 0;
			via.srclk = 1;
		}
	} else {
		t2shift = 0;
	}

	if (via.srb < 8) {
		switch (via.acr & 0x1c) {
		case 0x00:
			/* disabled */
			break;
		case 0x04:
			/* shift in under control of t2 */

			if (t2shift) {
				/* shifting in 0s since cb2 is always an output */

				via.sr <<= 1;
				via.srb++;
			}

			break;
		case 0x08:
			/* shift in under system clk control */

			via.sr <<= 1;
			via.srb++;

			break;
		case 0x0c:
			/* shift in under cb1 control */
			break;
		case 0x10:
			/* shift out under t2 control (free run) */

			if (t2shift) {
				via.cb2s = (via.sr >> 7) & 1;

				via.sr <<= 1;
				via.sr |= via.cb2s;
			}

			break;
		case 0x14:
			/* shift out under t2 control */

			if (t2shift) {
				via.cb2s = (via.sr >> 7) & 1;

				via.sr <<= 1;
				via.sr |= via.cb2s;
				via.srb++;
			}

			break;
		case 0x18:
			/* shift out under system clock control */

			via.cb2s = (via.sr >> 7) & 1;

			via.sr <<= 1;
			via.sr |= via.cb2s;
			via.srb++;

			break;
		case 0x1c:
			/* shift out under cb1 control */
			break;
		}

		if (via.srb == 8) {
			via.ifr |= 0x04;
			int_update ();
		}
	}
}

/* perform the second part of the via emulation */

static einline void via_sstep1 (void)
{
	if ((via.pcr & 0x0e) == 0x0a) {
		/* if ca2 is in pulse mode, then make sure
		 * it gets restored to '1' after the pulse.
		 */

		via.ca2 = 1;
	}

	if ((via.pcr & 0xe0) == 0xa0) {
		/* if cb2 is in pulse mode, then make sure
		 * it gets restored to '1' after the pulse.
		 */

		via.cb2h = 1;
	}
}

static einline void alg_addline (long x0, long y0, long x1, long y1, unsigned char color)
{
	unsigned long key;
	long index;

	key = (unsigned long) x0;
	key = key * 31 + (unsigned long) y0;
	key = key * 31 + (unsigned long) x1;
	key = key * 31 + (unsigned long) y1;
	key %= VECTOR_HASH;

	/* first check if the line to be drawn is in the current draw list.
	 * if it is, then it is not added again.
	 */

	index = vector_hash[key];

	if (index >= 0 && index < vector_draw_cnt &&
		x0 == vectors_draw[index].x0 &&
		y0 == vectors_draw[index].y0 &&
		x1 == vectors_draw[index].x1 &&
		y1 == vectors_draw[index].y1) {
		vectors_draw[index].color = color;
	} else {
		/* missed on the draw list, now check if the line to be drawn is in
		 * the erase list ... if it is, "invalidate" it on the erase list.
		 */

		if (index >= 0 && index < vector_erse_cnt &&
			x0 == vectors_erse[index].x0 &&
			y0 == vectors_erse[index].y0 &&
			x1 == vectors_erse[index].x1 &&
			y1 == vectors_erse[index].y1) {
			vectors_erse[index].color = VECTREX_COLORS;
		}

		vectors_draw[vector_draw_cnt].x0 = x0;
		vectors_draw[vector_draw_cnt].y0 = y0;
		vectors_draw[vector_draw_cnt].x1 = x1;
		vectors_draw[vector_draw_cnt].y1 = y1;
		vectors_draw[vector_draw_cnt].color = color;
		vector_hash[key] = vector_draw_cnt;
		vector_draw_cnt++;
	}
}

/* perform a single cycle worth of analog emulation */

static einline void alg_sstep (void)
{
	long sig_dx, sig_dy;
	unsigned sig_ramp;
	unsigned sig_blank;

	if ((via.acr & 0x10) == 0x10) {
		sig_blank = via.cb2s;
	} else {
		sig_blank = via.cb2h;
	}

	if (via.ca2 == 0) {
		/* need to force the current point to the 'orgin' so just
		 * calculate distance to origin and use that as dx,dy.
		 */

		sig_dx = ALG_MAX_X / 2 - alg.curr_x;
		sig_dy = ALG_MAX_Y / 2 - alg.curr_y;
	} else {
		if (via.acr & 0x80) {
			sig_ramp = via.t1pb7;
		} else {
			sig_ramp = via.orb & 0x80;
		}

		if (sig_ramp == 0) {
			sig_dx = alg.dx;
			sig_dy = alg.dy;
		} else {
			sig_dx = 0;
			sig_dy = 0;
		}
	}

	if (alg.vectoring == 0) {
		if (sig_blank == 1 &&
			alg.curr_x >= 0 && alg.curr_x < ALG_MAX_X &&
			alg.curr_y >= 0 && alg.curr_y < ALG_MAX_Y) {

			/* start a new vector */

			alg.vectoring = 1;
			alg.vector_x0 = alg.curr_x;
			alg.vector_y0 = alg.curr_y;
			alg.vector_x1 = alg.curr_x;
			alg.vector_y1 = alg.curr_y;
			alg.vector_dx = sig_dx;
			alg.vector_dy = sig_dy;
			alg.vector_color = (unsigned char) alg.zsh;
		}
	} else {
		/* already drawing a vector ... check if we need to turn it off */

		if (sig_blank == 0) {
			/* blank just went on, vectoring turns off, and we've got a
			 * new line.
			 */

			alg.vectoring = 0;

			alg_addline (alg.vector_x0, alg.vector_y0,
						 alg.vector_x1, alg.vector_y1,
						 alg.vector_color);
		} else if (sig_dx != alg.vector_dx ||
				   sig_dy != alg.vector_dy ||
				   (unsigned char) alg.zsh != alg.vector_color) {

			/* the parameters of the vectoring processing has changed.
			 * so end the current line.
			 */

			alg_addline (alg.vector_x0, alg.vector_y0,
						 alg.vector_x1, alg.vector_y1,
						 alg.vector_color);

			/* we continue vectoring with a new set of parameters if the
			 * current point is not out of limits.
			 */

			if (alg.curr_x >= 0 && alg.curr_x < ALG_MAX_X &&
				alg.curr_y >= 0 && alg.curr_y < ALG_MAX_Y) {
				alg.vector_x0 = alg.curr_x;
				alg.vector_y0 = alg.curr_y;
				alg.vector_x1 = alg.curr_x;
				alg.vector_y1 = alg.curr_y;
				alg.vector_dx = sig_dx;
				alg.vector_dy = sig_dy;
				alg.vector_color = (unsigned char) alg.zsh;
			} else {
				alg.vectoring = 0;
			}
		}
	}

	alg.curr_x += sig_dx;
	alg.curr_y += sig_dy;

	if (alg.vectoring == 1 &&
		alg.curr_x >= 0 && alg.curr_x < ALG_MAX_X &&
		alg.curr_y >= 0 && alg.curr_y < ALG_MAX_Y) {

		/* we're vectoring ... current point is still within limits so
		 * extend the current vector.
		 */

		alg.vector_x1 = alg.curr_x;
		alg.vector_y1 = alg.curr_y;
	}
}

void vecx_emu (long cycles)
{
	unsigned c, icycles;

	while (cycles > 0) {
		icycles = e6809_sstep (via.ifr & 0x80, 0);

		for (c = 0; c < icycles; c++) {
			via_sstep0 ();
			alg_sstep ();
			via_sstep1 ();
		}

		cycles -= (long) icycles;

		fcycles -= (long) icycles;

		if (fcycles < 0) {
			vector_t *tmp;

			fcycles += FCYCLES_INIT;
			Update_Video_Ingame();

			/* everything that was drawn during this pass now now enters
			 * the erase list for the next pass.
			 */

			vector_erse_cnt = vector_draw_cnt;
			vector_draw_cnt = 0;

			tmp = vectors_erse;
			vectors_erse = vectors_draw;
			vectors_draw = tmp;
		}
	}
}

#ifndef __E6809_H
#define __E6809_H

/* GPLv3 License - See LICENSE.md for more information. */

/* Gameblabla August 7th 2019 :
 * - Allow saving of registers. It's quite big though. Useful for save state
 * */

#include <stdint.h>

/* user defined read and write functions */

extern unsigned char (*e6809_read8) (unsigned address);
extern void (*e6809_write8) (unsigned address, unsigned char data);

void e6809_reset (void);
unsigned e6809_sstep (unsigned irq_i, unsigned irq_f);

extern void e6809_state(uint_fast8_t load, FILE* fp);

#endif

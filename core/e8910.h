#ifndef __E8910_H
#define __E8910_H

/* GPLv3 License - See LICENSE.md for more information. */

/* Gameblabla August 7th 2019 :
 * - Allow saving of registers. It's quite big though. Useful for save state
 * */

void e8910_init_sound();
void e8910_done_sound();
void e8910_write(int r, int v);

extern void e8910_state(uint_fast8_t load, FILE* fp);

#endif

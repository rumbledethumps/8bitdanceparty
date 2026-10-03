#ifndef GT2_H
#define GT2_H

#include <stdint.h>

/* GoatTracker 2 player for the C64 tune of adb-ntsc.prg */

void gt2_init(void);
void gt2_play(void);

/* $D400-$D418 as written by the last gt2_play() call */
extern uint8_t gt2_sid[25];

/* Current instrument of each voice */
extern uint8_t gt2_instr[3];

#endif

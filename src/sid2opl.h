#ifndef SID2OPL_H
#define SID2OPL_H

#include <stdint.h>

/* SID register image to OPL2, once per 60 Hz frame after gt2_play() */

/* Call after the OPL2 is enabled with all registers 0 */
void sid2opl_reset(void);

/* sid is $D400-$D418, instr is the GT2 instrument of each voice */
void sid2opl_update(const uint8_t *sid, const uint8_t *instr);

#endif

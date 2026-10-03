#include <rp6502.h>
#include "opl.h"
#include "xram.h"

void opl_write(uint8_t reg, uint8_t val)
{
    xram0_poke8(XRAM_OPL + reg, val);
}

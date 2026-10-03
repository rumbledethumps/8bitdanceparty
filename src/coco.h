#ifndef COCO_H
#define COCO_H

/* A tick of the CoCo player averages 2037.646 cycles of the 894886 Hz CPU
 * clock over a pass of the song. */
#define COCO_TICK_HZ_X1000 439176

void coco_reset(void);
void coco_tick(void);

#endif

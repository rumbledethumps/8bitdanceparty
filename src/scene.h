#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

/* A scene starts after the canvas is selected and the OPL2 is reset. */

void c64_scene_start(void);
void c64_scene_frame(uint8_t party);

void coco_scene_start(void);
void coco_scene_frame(uint8_t adb2);
void coco_scene_tick(void);

#endif

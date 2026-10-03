#ifndef COCO_SONG_H
#define COCO_SONG_H

#include <stdint.h>

/* Records of five bytes: duration in ticks, then a note index for each of
 * the four voices. Index 0 is a rest and index i is MIDI note 38 + i. A
 * duration of 0 ends the song. */
extern const uint8_t coco_song[];

/* OPL2 A0 and B0 register values (key bit clear) for each note index. */
extern const uint8_t coco_note_lo[];
extern const uint8_t coco_note_hi[];

#endif

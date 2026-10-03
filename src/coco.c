#include "coco.h"
#include "coco_song.h"
#include "opl.h"

#define CHANNELS 8
#define KEY_ON 0x20

static const uint8_t op_offset[CHANNELS] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11,
};

static const uint8_t patch_reg[] = {
    0x20, 0x23, 0x40, 0x43, 0x60, 0x63, 0x80, 0x83, 0xE0, 0xE3,
};

/* Values for patch_reg then C0, for channels 0-7. Voice v plays on channels
 * 2v and 2v+1 because a pair of channels matches the harmonics of the CoCo
 * wavetables much more closely than one channel does. The envelope values
 * give an instant attack, full sustain and a release of about 10 ms, which
 * ends a note without a click. */
static const uint8_t patch[] = {
    0x22, 0x21, 0x23, 0x05, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x00, /* square */
    0x22, 0x23, 0x17, 0x15, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x06,
    0x21, 0x22, 0x0A, 0x12, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x01, /* organ */
    0x23, 0x23, 0x3F, 0x12, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x01,
    0x22, 0x21, 0x23, 0x05, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x00, /* square */
    0x22, 0x23, 0x17, 0x15, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x00, 0x06,
    0x21, 0x21, 0x20, 0x00, 0xF0, 0xF0, 0x0C, 0x0C, 0x01, 0x01, 0x0E, /* saw */
    0x21, 0x22, 0x12, 0x1D, 0xF0, 0xF0, 0x0C, 0x0C, 0x00, 0x01, 0x0A,
};

static const uint8_t *song;
static uint8_t ticks;
static uint8_t note[4];

static void opl_write_pair(uint8_t reg, uint8_t val)
{
    opl_write(reg, val);
    opl_write(reg + 1, val);
}

void coco_reset(void)
{
    static uint8_t c, i, k;
    opl_write(0x01, 0x20);
    for (c = 0, k = 0; c < CHANNELS; ++c)
    {
        for (i = 0; i < sizeof(patch_reg); ++i)
            opl_write(patch_reg[i] + op_offset[c], patch[k++]);
        opl_write(0xC0 + c, patch[k++]);
    }
    for (i = 0; i < 4; ++i)
        note[i] = 0;
    song = coco_song;
    ticks = 1;
}

void coco_tick(void)
{
    static uint8_t v, n, c, hi;
    if (--ticks)
        return;
    ticks = song[0];
    /* The CoCo player does not reload the tick count at the end of the
     * song, so the last record plays for 256 more ticks before the song
     * repeats. */
    if (!ticks)
    {
        song = coco_song;
        return;
    }
    for (v = 0; v < 4; ++v)
    {
        n = song[v + 1];
        if (n == note[v])
            continue;
        c = v << 1;
        if (n)
        {
            opl_write_pair(0xA0 + c, coco_note_lo[n]);
            hi = coco_note_hi[n] | KEY_ON;
        }
        else
            hi = coco_note_hi[note[v]];
        opl_write_pair(0xB0 + c, hi);
        note[v] = n;
    }
    song += 5;
}

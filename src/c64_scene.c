#include <rp6502.h>
#include "gt2.h"
#include "scene.h"
#include "sid2opl.h"
#include "xram.h"

#define YELLOW 7
#define BALLS 8

/* The top left of the picture is at C64 sprite coordinates 24, 50 and at
 * canvas coordinates 0, 20. */
#define LEFT 24
#define TOP (50 - 20)

/* Mode 5 has no sprite enable, so a hidden ball is placed above the canvas. */
#define HIDDEN (-32)

static const int8_t sine64[64] = {
    0, 3, 6, 9, 11, 14, 17, 19, 21, 23, 25, 26, 28, 29, 29, 30,
    30, 30, 29, 29, 28, 26, 25, 23, 21, 19, 17, 14, 12, 9, 6, 3,
    0, -3, -6, -9, -11, -14, -17, -19, -21, -23, -25, -26, -28, -29, -29, -30,
    -30, -30, -29, -29, -28, -26, -25, -23, -21, -19, -17, -14, -12, -9, -6, -3,
};

static const int8_t sine128[128] = {
    0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 19, 20, 21, 21, 22, 22, 23, 23, 23, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 23, 23, 23, 22, 22, 21, 21, 20, 19, 19, 18,
    17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 2, 1,
    0, -1, -2, -3, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16,
    -17, -18, -19, -19, -20, -21, -21, -22, -22, -23, -23, -23, -24, -24, -24, -24,
    -24, -24, -24, -24, -24, -23, -23, -23, -22, -22, -21, -21, -20, -19, -19, -18,
    -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -2, -1,
};

/* Color numbers from $0CBB-$0CBF and $0BC0-$0BDC. Ball colors start 7
 * entries below the index. Below the start of the table, the 8-bit Y index
 * in the original wraps to $0CBB-$0CBF, which are screen RAM. */
static const uint8_t cycle[] = {
    12, 12, 0, 0, 8,
    6, 6, 14, 14, 3, 3, 13, 13, 7, 7, 15, 15, 10, 10, 2, 2,
    6, 6, 14, 14, 3, 3, 13, 13, 7, 7, 15, 15, 10,
};
#define CYCLE(i) cycle[(i) + 5]

static const mode3_config_t picture = {
    false, false, 0, 20, C64_WIDTH, C64_HEIGHT, XRAM_C64, XRAM_C64_PALETTE};

static const mode3_config_t title = {
    false, false, 120, 180, TITLE_WIDTH, TITLE_HEIGHT, XRAM_TITLE, XRAM_TITLE_PALETTE};

/* The C64 draws sprite 0 in front, and mode 5 draws the last sprite in
 * front, so sprite n is ball 7 - n. */
static mode5_sprite_t balls[BALLS];
static uint16_t palettes[BALLS][2];

static uint16_t rgb[16];
static uint16_t title_rgb;
static uint8_t phase_x, phase_y, skip, divider, idx;

static void colors(void)
{
    static uint8_t i;
    for (i = 0; i < BALLS; ++i)
        palettes[i][1] = rgb[CYCLE(idx - i)];
    xram0_write(XRAM_BALL_PALETTE, palettes, sizeof(palettes));
}

void c64_scene_start(void)
{
    static uint8_t i;
    phase_x = 0;
    phase_y = 40;
    skip = 0;
    divider = 1;
    idx = 7;
    xram0_read(rgb, XRAM_C64_PALETTE, sizeof(rgb));
    title_rgb = rgb[YELLOW];
    xram0_poke16(XRAM_TITLE_PALETTE + 2, title_rgb);
    colors();
    for (i = 0; i < BALLS; ++i)
    {
        balls[i].y_pos_px = HIDDEN;
        balls[i].xram_sprite_ptr = XRAM_BALL;
        balls[i].palette_ptr = XRAM_BALL_PALETTE + i * sizeof(palettes[0]);
    }
    xram0_write(XRAM_BALL_CONFIG, balls, sizeof(balls));
    xram0_write(XRAM_C64_CONFIG, &picture, sizeof(picture));
    xreg_vga_mode3(MODE3_4BPP, XRAM_C64_CONFIG, 0);
    xram0_write(XRAM_TITLE_CONFIG, &title, sizeof(title));
    xreg_vga_mode3(MODE3_1BPP, XRAM_TITLE_CONFIG, 1, 180, 180 + TITLE_HEIGHT);
    xreg_vga_mode5(MODE5_1BPP | MODE5_32X32, XRAM_BALL_CONFIG, BALLS, 1);
    sid2opl_reset();
    gt2_init();
}

void c64_scene_frame(uint8_t party)
{
    static uint8_t i, p, q;
    static int8_t s;

    /* Ball positions are written first, which keeps those writes in the
     * vertical blank. */
    phase_x = (phase_x + 1) & 127;
    if (++skip == 53)
        skip = 0;
    else
    {
        if (++divider == 3)
        {
            divider = 0;
            colors();
            title_rgb = rgb[CYCLE(idx + 8)];
            if (--idx == 1)
                idx = 20;
        }
        phase_y = (phase_y + 1) & 127;
    }

    p = phase_x;
    q = phase_y;
    for (i = BALLS; i--;)
    {
        p = (p + 5) & 127;
        q = (q + 5) & 127;
        s = sine64[p & 63];
        /* In the original, the second add includes the carry flag of
         * 180 + sine64. */
        balls[i].x_pos_px = 180 - LEFT + s + sine128[p] + (s < 0);
        balls[i].y_pos_px = party ? 120 - TOP + sine128[q] : HIDDEN;
    }
    xram0_write(XRAM_BALL_CONFIG, balls, sizeof(balls));
    xram0_poke16(XRAM_TITLE_PALETTE + 2, party ? title_rgb : rgb[YELLOW]);

    gt2_play();
    sid2opl_update(gt2_sid, gt2_instr);
}

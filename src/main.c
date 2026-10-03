#include <rp6502.h>
#include "coco.h"
#include "scene.h"
#include "xram.h"

#define C64 0
#define COCO 1

#define ENTER 1
#define SPACE 2

static uint8_t scene;
static uint8_t sub[2] = {1, 0};
static keyboard_t keyboard;
static gamepad_t gamepad;

/* volatile has no effect on the cc65 optimizer, which removes the read of
 * T1C-L that clears the timer flag. */
#ifdef __CC65__
#pragma optimize(push, off)
#endif

/* The 65C22 repeats timer 1 every latch + 2 cycles. Both sides of the
 * division are 8 times smaller so the product fits in 32 bits. */
static void timer_start(void)
{
    static uint16_t latch;
    latch = (uint32_t)ria_attr_get(RIA_ATTR_PHI2_KHZ) * (1000000 / 8) /
                (COCO_TICK_HZ_X1000 / 8) -
            2;
    VIA.acr = 0x40;
    VIA.t1_lo = latch;
    VIA.t1_hi = latch >> 8;
}

static uint8_t timer_elapsed(void)
{
    if (!(VIA.ifr & 0x40))
        return 0;
    VIA.t1_lo;
    return 1;
}

#ifdef __CC65__
#pragma optimize(pop)
#endif

static uint8_t buttons(void)
{
    static uint8_t b, i;
    xram0_read(&keyboard, XRAM_KEYBOARD, sizeof(keyboard));
    xram0_read(&gamepad, XRAM_GAMEPAD, sizeof(gamepad));
    b = 0;
    if (KEYBOARD_PRESSED(keyboard.keys, HID_KEY_ENTER) ||
        KEYBOARD_PRESSED(keyboard.keys, HID_KEY_KEYPAD_ENTER))
        b = ENTER;
    if (KEYBOARD_PRESSED(keyboard.keys, HID_KEY_SPACE))
        b |= SPACE;
    for (i = 0; i < GAMEPAD_PLAYERS; ++i)
    {
        if (gamepad.player[i].btn1 & (GAMEPAD_BTN1_SELECT | GAMEPAD_BTN1_START))
            b |= ENTER;
        if (gamepad.player[i].btn0 & (GAMEPAD_BTN0_A | GAMEPAD_BTN0_B |
                                      GAMEPAD_BTN0_X | GAMEPAD_BTN0_Y))
            b |= SPACE;
    }
    return b;
}

static void start(void)
{
    xreg_vga_canvas(CANVAS_320X240);
    xreg_ria_opl(XRAM_OPL);
    if (scene == C64)
        c64_scene_start();
    else
        coco_scene_start();
}

int main(void)
{
    static uint8_t vsync, held, b;

    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);
    /* Enter may still be held down from the command that started the
     * program. */
    held = buttons();
    timer_start();
    start();
    vsync = ria_vsync();
    for (;;)
    {
        /* Polling keeps every OPL2 write in one context. */
        if (timer_elapsed() && scene == COCO)
            coco_scene_tick();
        if (ria_vsync() == vsync)
            continue;
        vsync = ria_vsync();
        b = buttons();
        if (b & ~held & ENTER)
        {
            scene ^= 1;
            start();
        }
        if (b & ~held & SPACE)
            sub[scene] ^= 1;
        held = b;
        if (scene == C64)
            c64_scene_frame(sub[C64]);
        else
            coco_scene_frame(sub[COCO]);
    }
}

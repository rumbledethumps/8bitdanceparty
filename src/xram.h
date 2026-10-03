/* RP6502 XRAM Mapping */

#ifndef XRAM_H
#define XRAM_H

#include <rp6502.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* VGA canvas */

#define xreg_vga_canvas(...) xreg(1, 0, 0, __VA_ARGS__)

#define CANVAS_CONSOLE 0
#define CANVAS_320X240 1
#define CANVAS_320X180 2
#define CANVAS_640X480 3
#define CANVAS_640X360 4

/* VGA mode 3 */

#define xreg_vga_mode3(...) xreg(1, 0, 1, 3, __VA_ARGS__)

#define MODE3_1BPP 0x00
#define MODE3_2BPP 0x01
#define MODE3_4BPP 0x02
#define MODE3_8BPP 0x03
#define MODE3_16BPP 0x04

#define MODE3_REVERSE_BITS 0x08

typedef struct
{
    bool x_wrap;
    bool y_wrap;
    int16_t x_pos_px;
    int16_t y_pos_px;
    int16_t width_px;
    int16_t height_px;
    uint16_t xram_data_ptr;
    uint16_t xram_palette_ptr;
} mode3_config_t; /* layout */

/* VGA mode 5 */

#define xreg_vga_mode5(...) xreg(1, 0, 1, 5, __VA_ARGS__)

#define MODE5_1BPP 0x00
#define MODE5_2BPP 0x01
#define MODE5_4BPP 0x02
#define MODE5_8BPP 0x03

#define MODE5_8X8 0x00
#define MODE5_16X16 0x08
#define MODE5_32X32 0x10
#define MODE5_64X64 0x18
#define MODE5_128X128 0x20
#define MODE5_256X256 0x28
#define MODE5_512X512 0x30
#define MODE5_CUSTOM 0x38

#define MODE5_HFLIP 0x10
#define MODE5_VFLIP 0x20
#define MODE5_HDOUBLE 0x40
#define MODE5_VDOUBLE 0x80

#define MODE5_IMAGE(bpp, size)                \
    struct                                    \
    {                                         \
        struct                                \
        {                                     \
            uint8_t cols[(size) * (bpp) / 8]; \
        } rows[size];                         \
    }

#define MODE5_SIZE(width, height) \
    ((((height) / 4 - 1) << 4) | ((width) / 4 - 1))

#define MODE5_CUSTOM_IMAGE(bpp, width, height)       \
    struct                                           \
    {                                                \
        struct                                       \
        {                                            \
            uint8_t cols[((width) * (bpp) + 7) / 8]; \
        } rows[height];                              \
    }

typedef struct
{
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} mode5_sprite_t; /* layout */

typedef struct
{
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
    uint8_t width_height;
    uint8_t options;
} mode5_csprite_t; /* layout */

/* Keyboard */

#define KEYBOARD_NO_KEY 0
#define KEYBOARD_NUM_LOCK 1
#define KEYBOARD_CAPS_LOCK 2
#define KEYBOARD_SCROLL_LOCK 3

#define KEYBOARD_PRESSED(keys, code) ((keys)[(code) >> 3] & (1 << ((code) & 7)))

#define xreg_ria_keyboard(...) xreg(0, 0, 0, __VA_ARGS__)

typedef struct
{
    uint8_t keys[32];
} keyboard_t; /* layout */

#define HID_KEY_ENTER 0x28
#define HID_KEY_SPACE 0x2C
#define HID_KEY_KEYPAD_ENTER 0x58

/* Gamepads */

#define GAMEPAD_PLAYERS 4

#define GAMEPAD_DPAD_UP 0x01
#define GAMEPAD_DPAD_DOWN 0x02
#define GAMEPAD_DPAD_LEFT 0x04
#define GAMEPAD_DPAD_RIGHT 0x08

#define GAMEPAD_FEAT_TYPE_MASK 0x30
#define GAMEPAD_TYPE_UNKNOWN 0x00
#define GAMEPAD_TYPE_WESTERN 0x10
#define GAMEPAD_TYPE_EASTERN 0x20
#define GAMEPAD_TYPE_PLAYSTATION 0x30
#define GAMEPAD_FEAT_STICKS 0x40
#define GAMEPAD_FEAT_CONNECTED 0x80

#define GAMEPAD_LSTICK_UP 0x01
#define GAMEPAD_LSTICK_DOWN 0x02
#define GAMEPAD_LSTICK_LEFT 0x04
#define GAMEPAD_LSTICK_RIGHT 0x08
#define GAMEPAD_RSTICK_UP 0x10
#define GAMEPAD_RSTICK_DOWN 0x20
#define GAMEPAD_RSTICK_LEFT 0x40
#define GAMEPAD_RSTICK_RIGHT 0x80

#define GAMEPAD_BTN0_A 0x01
#define GAMEPAD_BTN0_B 0x02
#define GAMEPAD_BTN0_C 0x04
#define GAMEPAD_BTN0_X 0x08
#define GAMEPAD_BTN0_Y 0x10
#define GAMEPAD_BTN0_Z 0x20
#define GAMEPAD_BTN0_L1 0x40
#define GAMEPAD_BTN0_R1 0x80

#define GAMEPAD_BTN1_L2 0x01
#define GAMEPAD_BTN1_R2 0x02
#define GAMEPAD_BTN1_SELECT 0x04
#define GAMEPAD_BTN1_START 0x08
#define GAMEPAD_BTN1_HOME 0x10
#define GAMEPAD_BTN1_L3 0x20
#define GAMEPAD_BTN1_R3 0x40

#define xreg_ria_gamepad(...) xreg(0, 0, 2, __VA_ARGS__)

typedef struct
{
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    int8_t lx;
    int8_t ly;
    int8_t rx;
    int8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_player_t;

typedef struct
{
    gamepad_player_t player[GAMEPAD_PLAYERS];
} gamepad_t; /* layout */

/* OPL2 */

#define xreg_ria_opl(...) xreg(0, 1, 1, __VA_ARGS__)

typedef struct
{
    uint8_t reg[256];
} opl_t; /* layout */

/* After the XRAM_ names: OPL_PAGE_CHECK(XRAM_OPL); */
#define OPL_PAGE_CHECK(addr) \
    _Static_assert((addr) % 256 == 0, #addr " does not start a page.")

/* Pictures */

#define C64_WIDTH 320
#define C64_HEIGHT 200
#define TITLE_WIDTH 160
#define TITLE_HEIGHT 16
#define COCO_WIDTH 256
#define COCO_HEADER_ROWS 90
#define COCO_BODY_ROWS 102

typedef MODE5_IMAGE(1, 32) ball_image_t;

/* Layout */

typedef struct
{
    opl_t opl;
    uint8_t c64[C64_WIDTH / 2 * C64_HEIGHT];
    uint8_t coco_header1[COCO_WIDTH / 4 * COCO_HEADER_ROWS];
    uint8_t coco_header2[COCO_WIDTH / 4 * COCO_HEADER_ROWS];
    uint8_t coco_body1[COCO_WIDTH / 4 * COCO_BODY_ROWS];
    uint8_t coco_body2[COCO_WIDTH / 4 * COCO_BODY_ROWS];
    uint8_t title[TITLE_WIDTH / 8 * TITLE_HEIGHT];
    ball_image_t ball;
    uint16_t c64_palette[16];
    uint16_t title_palette[2];
    uint16_t ball_palette[8][2];
    uint16_t coco_palette[4];
    mode3_config_t c64_config;
    mode3_config_t title_config;
    mode5_sprite_t ball_config[8];
    mode3_config_t fill_config;
    mode3_config_t header_config;
    mode3_config_t body_config;
    keyboard_t keyboard;
    gamepad_t gamepad;
} xram_layout_t;

#define XRAM_OPL offsetof(xram_layout_t, opl)
#define XRAM_C64 offsetof(xram_layout_t, c64)
#define XRAM_COCO_HEADER1 offsetof(xram_layout_t, coco_header1)
#define XRAM_COCO_HEADER2 offsetof(xram_layout_t, coco_header2)
#define XRAM_COCO_BODY1 offsetof(xram_layout_t, coco_body1)
#define XRAM_COCO_BODY2 offsetof(xram_layout_t, coco_body2)
#define XRAM_TITLE offsetof(xram_layout_t, title)
#define XRAM_BALL offsetof(xram_layout_t, ball)
#define XRAM_C64_PALETTE offsetof(xram_layout_t, c64_palette)
#define XRAM_TITLE_PALETTE offsetof(xram_layout_t, title_palette)
#define XRAM_BALL_PALETTE offsetof(xram_layout_t, ball_palette)
#define XRAM_COCO_PALETTE offsetof(xram_layout_t, coco_palette)
#define XRAM_C64_CONFIG offsetof(xram_layout_t, c64_config)
#define XRAM_TITLE_CONFIG offsetof(xram_layout_t, title_config)
#define XRAM_BALL_CONFIG offsetof(xram_layout_t, ball_config)
#define XRAM_FILL_CONFIG offsetof(xram_layout_t, fill_config)
#define XRAM_HEADER_CONFIG offsetof(xram_layout_t, header_config)
#define XRAM_BODY_CONFIG offsetof(xram_layout_t, body_config)
#define XRAM_KEYBOARD offsetof(xram_layout_t, keyboard)
#define XRAM_GAMEPAD offsetof(xram_layout_t, gamepad)

OPL_PAGE_CHECK(XRAM_OPL);

#endif

#include <rp6502.h>
#include "coco.h"
#include "scene.h"
#include "xram.h"

#define LEFT 32
#define TOP 24

/* Row 0 of every CoCo picture is white, so a wrap of that row fills the
 * canvas with white. */
static const mode3_config_t fill = {
    true, true, 0, 0, COCO_WIDTH, 1, XRAM_COCO_HEADER1, XRAM_COCO_PALETTE};

static const mode3_config_t header = {
    false, false, LEFT, TOP, COCO_WIDTH, COCO_HEADER_ROWS,
    XRAM_COCO_HEADER1, XRAM_COCO_PALETTE};

static const mode3_config_t body = {
    false, false, LEFT, TOP + COCO_HEADER_ROWS, COCO_WIDTH, COCO_BODY_ROWS,
    XRAM_COCO_BODY1, XRAM_COCO_PALETTE};

static uint16_t tick_count;

void coco_scene_start(void)
{
    xram0_write(XRAM_FILL_CONFIG, &fill, sizeof(fill));
    xreg_vga_mode3(MODE3_2BPP, XRAM_FILL_CONFIG, 0);
    xram0_write(XRAM_HEADER_CONFIG, &header, sizeof(header));
    xreg_vga_mode3(MODE3_2BPP, XRAM_HEADER_CONFIG, 1,
                   TOP, TOP + COCO_HEADER_ROWS);
    xram0_write(XRAM_BODY_CONFIG, &body, sizeof(body));
    xreg_vga_mode3(MODE3_2BPP, XRAM_BODY_CONFIG, 1,
                   TOP + COCO_HEADER_ROWS, TOP + COCO_HEADER_ROWS + COCO_BODY_ROWS);
    coco_reset();
    tick_count = 0;
}

void coco_scene_frame(uint8_t adb2)
{
    xram0_poke16(XRAM_HEADER_CONFIG + offsetof(mode3_config_t, xram_data_ptr),
                 adb2 ? XRAM_COCO_HEADER2 : XRAM_COCO_HEADER1);
    xram0_poke16(XRAM_BODY_CONFIG + offsetof(mode3_config_t, xram_data_ptr),
                 tick_count & 0x100 ? XRAM_COCO_BODY2 : XRAM_COCO_BODY1);
}

void coco_scene_tick(void)
{
    coco_tick();
    ++tick_count;
}

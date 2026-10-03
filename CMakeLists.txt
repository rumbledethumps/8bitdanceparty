cmake_minimum_required(VERSION 3.21)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(danceparty C)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ART ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(BIN ${CMAKE_CURRENT_BINARY_DIR})

# png2xram(<name> <png> <bpp> [PALETTE] [ROWS <first> <count>]) writes
# <name>.bin and, with PALETTE, <name>_pal.bin.
function(png2xram name png bpp)
    cmake_parse_arguments(PARSE_ARGV 3 arg "PALETTE" "" "ROWS")
    set(outputs ${BIN}/${name}.bin)
    set(args --bpp ${bpp} --data ${BIN}/${name}.bin)
    if(arg_PALETTE)
        list(APPEND outputs ${BIN}/${name}_pal.bin)
        list(APPEND args --palette ${BIN}/${name}_pal.bin)
    endif()
    if(arg_ROWS)
        list(APPEND args --rows ${arg_ROWS})
    endif()
    add_custom_command(
        OUTPUT ${outputs}
        COMMAND ${Python3_EXECUTABLE} ${ART}/png2xram.py ${ART}/${png} ${args}
        DEPENDS ${ART}/png2xram.py ${ART}/${png}
        VERBATIM
    )
endfunction()

png2xram(c64 c64.png 4 PALETTE)
png2xram(title c64_title.png 1 PALETTE)
png2xram(ball ball.png 1)
png2xram(coco_header1 coco1.png 2 ROWS 0 90 PALETTE)
png2xram(coco_header2 coco_header2.png 2)
png2xram(coco_body1 coco1.png 2 ROWS 90 102)
png2xram(coco_body2 coco2.png 2 ROWS 90 102)

add_executable(danceparty)
rp6502_map(danceparty src/xram.h "XRAM_.*")
rp6502_asset(danceparty help src/help.txt)
rp6502_asset(danceparty XRAM(XRAM_C64) ${BIN}/c64.bin)
rp6502_asset(danceparty XRAM(XRAM_C64_PALETTE) ${BIN}/c64_pal.bin)
rp6502_asset(danceparty XRAM(XRAM_TITLE) ${BIN}/title.bin)
rp6502_asset(danceparty XRAM(XRAM_TITLE_PALETTE) ${BIN}/title_pal.bin)
rp6502_asset(danceparty XRAM(XRAM_BALL) ${BIN}/ball.bin)
rp6502_asset(danceparty XRAM(XRAM_COCO_HEADER1) ${BIN}/coco_header1.bin)
rp6502_asset(danceparty XRAM(XRAM_COCO_HEADER2) ${BIN}/coco_header2.bin)
rp6502_asset(danceparty XRAM(XRAM_COCO_BODY1) ${BIN}/coco_body1.bin)
rp6502_asset(danceparty XRAM(XRAM_COCO_BODY2) ${BIN}/coco_body2.bin)
rp6502_asset(danceparty XRAM(XRAM_COCO_PALETTE) ${BIN}/coco_header1_pal.bin)
rp6502_executable(danceparty DATA default RESET default)
rp6502_web(danceparty CONFIG [[
    title: '8-Bit Dance Party',
    run: 'onaudio',
    footer: 'Enter to switch main, space to switch sub.',
]])
target_sources(danceparty PRIVATE
    src/c64_scene.c
    src/coco.c
    src/coco_scene.c
    src/coco_song.c
    src/gt2.c
    src/gt2_song.c
    src/main.c
    src/opl.c
    src/sid2opl.c
)

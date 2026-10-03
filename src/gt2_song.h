#ifndef GT2_SONG_H
#define GT2_SONG_H

#include <stdint.h>

/* Player parameters set by the GoatTracker 2 relocator */
#define GT2_DEFAULTTEMPO 0x05
#define GT2_HR_SR 0x00
#define GT2_HR_AD 0x0f
#define GT2_FIRSTNOHRINSTR 0x0d
#define GT2_FIRSTLEGATOINSTR 0x0d

#define GT2_NUM_PATTERNS 98
#define GT2_NUM_INSTR 14

/* Instrument, wave, pulse, filter and speed tables are indexed from 1
 * as in GoatTracker 2; entry 0 is 0. */
extern const uint16_t gt2_freqtbl[96];
extern const uint8_t gt2_orders[];
extern const uint8_t gt2_orderofs[3];
extern const uint8_t gt2_patterns[];
extern const uint16_t gt2_pattofs[GT2_NUM_PATTERNS];
extern const uint8_t gt2_ins_ad[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_sr[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_waveptr[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_pulseptr[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_filtptr[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_vibparam[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_vibdelay[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_gatetimer[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_ins_firstwave[GT2_NUM_INSTR + 1];
extern const uint8_t gt2_wavel[48];
extern const uint8_t gt2_waver[48];
extern const uint8_t gt2_pulsel[35];
extern const uint8_t gt2_pulser[35];
extern const uint8_t gt2_filtl[39];
extern const uint8_t gt2_filtr[39];
extern const uint8_t gt2_speedl[8];
extern const uint8_t gt2_speedr[8];

struct gt2_state {
    /* Cleared by song_restart() */
    uint8_t songptr[3], trans[3], repeat[3], pattptr[3], packedrest[3];
    uint8_t newfx[3], newparam[3];
    uint8_t fx[3], param[3], newnote[3], waveptr[3], wave[3];
    uint8_t pulseptr[3], pulsetime[3];
    /* Kept by song_restart(), except tempo, counter, filtstep and filtctrl */
    uint8_t pattnum[3], tempo[3], counter[3], note[3], gate[3];
    uint8_t vibtime[3], vibdelay[3], wavetime[3], gatetimer[3], lastnote[3];
    uint8_t ad[3], sr[3];
    uint16_t freq[3], pulse[3];
    uint8_t filtstep, filttime, filtcutoff, filtctrl, filttype, mastervol;
    uint8_t funktempo[2];
    /* $D416-$D418 as computed by the last play call */
    uint8_t sid_cutoff, sid_filtctrl, sid_filttype;
};

/* State of adb-ntsc.prg as loaded */
extern const struct gt2_state gt2_pristine;

#endif

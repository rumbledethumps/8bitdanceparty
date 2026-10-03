#include <stddef.h>
#include <string.h>
#include "gt2.h"
#include "gt2_song.h"

#ifdef __CC65__
/* cc65 keeps locals on a slow software stack; the player is not reentrant. */
#pragma static-locals(on)
#endif

/* Pattern bytes */
#define FX 0x40
#define FXONLY 0x50
#define NOTE 0x60
#define REST 0xbd
#define PACKEDREST 0xc0

/* Orderlist bytes; LOOP also marks jumps in wave, pulse and filter programs */
#define REPEAT 0xd0
#define TRANSDOWN 0xe0
#define TRANS 0xf0
#define LOOP 0xff

#define TONEPORTA 0x3

#define NEXT_STEP(l, r, y) ((l)[(y) + 1] == LOOP ? (r)[(y) + 1] : (y) + 1)

uint8_t gt2_sid[25];
uint8_t gt2_instr[3];

static struct gt2_state p;
static uint8_t restart;
static uint8_t c;

static void set_note(uint8_t note)
{
    p.lastnote[c] = note;
    p.vibtime[c] = 0;
    p.freq[c] = gt2_freqtbl[note];
}

static void set_tempo(uint8_t tempo)
{
    if (tempo & 0x80)
        p.tempo[c] = tempo & 0x7f;
    else
        p.tempo[0] = p.tempo[1] = p.tempo[2] = tempo;
}

/* Row commands at tick 0, and wave program commands 5-F */
static void tick0_command(uint8_t cmd, uint8_t param)
{
    switch (cmd)
    {
    case 0x0:
        p.param[c] = gt2_ins_vibparam[gt2_instr[c]];
        p.fx[c] = cmd;
        break;
    case 0x1:
    case 0x2:
        p.vibtime[c] = 0;
        /* fall through */
    case 0x3:
    case 0x4:
        p.param[c] = param;
        p.fx[c] = cmd;
        break;
    case 0x5:
        p.ad[c] = param;
        break;
    case 0x6:
        p.sr[c] = param;
        break;
    case 0x7:
        p.wave[c] = param;
        break;
    case 0x8:
        p.waveptr[c] = param;
        p.wavetime[c] = 0;
        break;
    case 0x9:
        p.pulseptr[c] = param;
        p.pulsetime[c] = 0;
        break;
    case 0xa:
        p.filttime = 0;
        p.filtstep = param;
        break;
    case 0xb:
        p.filtctrl = param;
        if (!param)
            p.filtstep = 0;
        break;
    case 0xc:
        p.filtcutoff = param;
        break;
    case 0xd:
        p.mastervol = param;
        break;
    case 0xe:
        p.funktempo[0] = gt2_speedl[param];
        p.funktempo[1] = gt2_speedr[param];
        set_tempo(0);
        break;
    default:
        set_tempo(param);
    }
}

/* Continuous effects, and wave program commands 0-4 */
static void effect(uint8_t fx, uint8_t param)
{
    uint8_t left = gt2_speedl[param];
    uint8_t t;
    uint16_t speed, target, offset;

    if (left & 0x80)
    {
        t = p.lastnote[c];
        speed = (gt2_freqtbl[t + 1] - gt2_freqtbl[t]) >> gt2_speedr[param];
    }
    else
        speed = (uint16_t)left << 8 | gt2_speedr[param];

    switch (fx)
    {
    case 0x0:
        /* Off for parameter 0, and as in the 6502 player also for a normal
         * speed with a zero low byte */
        if ((left & 0x80) ? !param : !(uint8_t)speed)
            return;
        if (p.vibdelay[c])
        {
            --p.vibdelay[c];
            return;
        }
        /* fall through */
    case 0x4:
        if (!(left & 0x80))
            speed &= 0xff;
        t = p.vibtime[c];
        if (!(t & 0x80) && t > (left & 0x7f))
            t ^= 0xff;
        t += 2;
        p.vibtime[c] = t;
        if (t & 1)
            p.freq[c] -= speed;
        else
            p.freq[c] += speed;
        return;
    case 0x1:
        p.freq[c] += speed;
        return;
    case 0x2:
        p.freq[c] -= speed;
        return;
    }

    /* Toneportamento with speed 0 ties the note. The slide ends when the
     * 16-bit distance between the frequency and the target changes sign. */
    if (param)
    {
        target = gt2_freqtbl[p.note[c]];
        offset = p.freq[c] - target;
        if (p.freq[c] >= target)
        {
            if (!((uint16_t)(offset - speed) & 0x8000))
            {
                p.freq[c] -= speed;
                return;
            }
        }
        else if ((uint16_t)(offset + speed) & 0x8000)
        {
            p.freq[c] += speed;
            return;
        }
    }
    set_note(p.note[c]);
}

static void wave_exec(void)
{
    uint8_t y = p.waveptr[c];
    uint8_t left, right;

    if (y)
    {
        left = gt2_wavel[y];
        if (left < 0x10)
        {
            /* A delay ends when the counter matches or wraps to 0. */
            if (left != p.wavetime[c] && ++p.wavetime[c])
            {
                effect(p.fx[c], p.param[c]);
                return;
            }
        }
        else if (left < 0xf0)
            p.wave[c] = left - 0x10;
        p.waveptr[c] = NEXT_STEP(gt2_wavel, gt2_waver, y);
        p.wavetime[c] = 0;
        right = gt2_waver[y];
        if (left >= 0xe0)
        {
            left &= 0x0f;
            if (left < 0x5)
                effect(left, right);
            else
                tick0_command(left, right);
            return;
        }
        if (right)
        {
            if (right & 0x80)
                right = (right + p.note[c]) & 0x7f;
            set_note(right);
            return;
        }
    }
    effect(p.fx[c], p.param[c]);
}

static void pulse_exec(void)
{
    uint8_t y = p.pulseptr[c];
    uint8_t t;

    if (!y)
        return;
    if (!p.pulsetime[c])
    {
        t = gt2_pulsel[y];
        if (t & 0x80)
        {
            p.pulse[c] = (uint16_t)t << 8 | gt2_pulser[y];
            p.pulseptr[c] = NEXT_STEP(gt2_pulsel, gt2_pulser, y);
            return;
        }
        p.pulsetime[c] = t;
    }
    t = gt2_pulser[y];
    if (t & 0x80)
        p.pulse[c] -= 0x100;
    p.pulse[c] += t;
    if (!--p.pulsetime[c])
        p.pulseptr[c] = NEXT_STEP(gt2_pulsel, gt2_pulser, y);
}

static void filter_exec(void)
{
    uint8_t y = p.filtstep;
    uint8_t t;

    if (y)
    {
        if (!p.filttime)
        {
            t = gt2_filtl[y];
            if (t & 0x80)
            {
                p.filttype = t << 1;
                p.filtctrl = gt2_filtr[y];
                if (!gt2_filtl[y + 1])
                    p.filtcutoff = gt2_filtr[++y];
            }
            else if (t)
                p.filttime = t;
            else
                p.filtcutoff = gt2_filtr[y];
        }
        if (p.filttime)
        {
            p.filtcutoff += gt2_filtr[y];
            --p.filttime;
        }
        if (!p.filttime)
            p.filtstep = NEXT_STEP(gt2_filtl, gt2_filtr, y);
    }
    p.sid_cutoff = p.filtcutoff;
    p.sid_filtctrl = p.filtctrl;
    p.sid_filttype = p.filttype | p.mastervol;
}

static void sequencer(void)
{
    const uint8_t *order = gt2_orders + gt2_orderofs[c];
    uint8_t y = p.songptr[c];
    uint8_t b = order[y];

    if (b == LOOP)
    {
        y = order[y + 1];
        b = order[y];
    }
    if (b >= TRANSDOWN)
    {
        p.trans[c] = b - TRANS;
        b = order[++y];
    }
    if (b >= REPEAT)
    {
        /* The previous pattern plays again until the repeat counter equals
         * the count. */
        if (++p.repeat[c] != (uint8_t)(b - REPEAT))
            return;
        p.repeat[c] = 0;
    }
    else
        p.pattnum[c] = b;
    p.songptr[c] = y + 1;
}

/* Runs gatetimer frames before the first tick of the next row */
static void pattern_fetch(void)
{
    const uint8_t *patt = gt2_patterns + gt2_pattofs[p.pattnum[c]];
    uint8_t y = p.pattptr[c];
    uint8_t b = patt[y];

    if (b >= PACKEDREST)
    {
        if (!p.packedrest[c])
            p.packedrest[c] = b;
        if (++p.packedrest[c])
            return;
    }
    else
    {
        if (b < FX)
        {
            gt2_instr[c] = b;
            b = patt[++y];
        }
        if (b < NOTE)
        {
            p.newfx[c] = b & 0x0f;
            if (p.newfx[c])
                p.newparam[c] = patt[++y];
            if (b >= FXONLY)
                goto rest;
            b = patt[++y];
        }
        if (b > REST)
            p.gate[c] = b | 0xf0;
        else if (b < REST)
        {
            p.newnote[c] = b + p.trans[c];
            if (p.newfx[c] != TONEPORTA)
            {
                /* Instruments are ordered: hard restart, no hard restart, legato. */
                if (gt2_instr[c] < GT2_FIRSTNOHRINSTR)
                {
                    p.sr[c] = GT2_HR_SR;
                    p.ad[c] = GT2_HR_AD;
                    p.gate[c] = 0xfe;
                }
                else if (gt2_instr[c] < GT2_FIRSTLEGATOINSTR)
                    p.gate[c] = 0xfe;
            }
        }
    }
rest:
    p.pattptr[c] = patt[++y] ? y : 0;
}

static void note_init(uint8_t ins)
{
    uint8_t t = gt2_ins_firstwave[ins];

    if (t)
    {
        /* First wave $FE/$FF sets only the gate. */
        if (t < 0xfe)
        {
            p.wave[c] = t;
            t = 0xff;
        }
        p.gate[c] = t;
    }
    if ((t = gt2_ins_pulseptr[ins]))
    {
        p.pulseptr[c] = t;
        p.pulsetime[c] = 0;
    }
    if ((t = gt2_ins_filtptr[ins]))
    {
        p.filtstep = t;
        p.filttime = 0;
    }
    p.waveptr[c] = gt2_ins_waveptr[ins];
    p.sr[c] = gt2_ins_sr[ins];
    p.ad[c] = gt2_ins_ad[ins];
}

/* Returns nonzero when a new note started, which ends the frame for the voice. */
static uint8_t tick0(void)
{
    uint8_t ins;

    if (!p.pattptr[c])
        sequencer();
    ins = gt2_instr[c];
    p.gatetimer[c] = gt2_ins_gatetimer[ins];
    if (p.newnote[c])
    {
        p.note[c] = p.newnote[c] - NOTE;
        p.fx[c] = 0;
        p.newnote[c] = 0;
        p.vibdelay[c] = gt2_ins_vibdelay[ins];
        p.param[c] = gt2_ins_vibparam[ins];
        if (p.newfx[c] != TONEPORTA)
        {
            note_init(ins);
            tick0_command(p.newfx[c], p.newparam[c]);
            return 1;
        }
    }
    tick0_command(p.newfx[c], p.newparam[c]);
    return 0;
}

static void chn_exec(void)
{
    uint8_t t;

    if (!--p.counter[c])
    {
        if (tick0())
            return;
    }
    else if (p.counter[c] & 0x80)
    {
        t = p.tempo[c];
        if (t < 2)
        {
            p.tempo[c] = t ^ 1;
            t = p.funktempo[t] - 1;
        }
        p.counter[c] = t;
    }
    wave_exec();
    pulse_exec();
    if (p.counter[c] == p.gatetimer[c])
        pattern_fetch();
}

static void song_restart(void)
{
    memset(&p, 0, offsetof(struct gt2_state, pattnum));
    p.filtctrl = 0;
    p.filtstep = 0;
    for (c = 0; c < 3; ++c)
    {
        p.tempo[c] = GT2_DEFAULTTEMPO;
        p.counter[c] = 1;
        gt2_instr[c] = 1;
    }
    restart = 0;
}

void gt2_init(void)
{
    memcpy(&p, &gt2_pristine, sizeof(p));
    memset(gt2_sid, 0, sizeof(gt2_sid));
    restart = 1;
}

/* Constant indexes keep this cheap on the 6502. */
#define SID_VOICE(v)                                \
    do                                              \
    {                                               \
        gt2_sid[7 * (v) + 0] = (uint8_t)p.freq[v];  \
        gt2_sid[7 * (v) + 1] = p.freq[v] >> 8;      \
        gt2_sid[7 * (v) + 2] = (uint8_t)p.pulse[v]; \
        gt2_sid[7 * (v) + 3] = p.pulse[v] >> 8;     \
        gt2_sid[7 * (v) + 4] = p.wave[v] & p.gate[v]; \
        gt2_sid[7 * (v) + 5] = p.ad[v];             \
        gt2_sid[7 * (v) + 6] = p.sr[v];             \
    } while (0)

void gt2_play(void)
{
    /* The restart call leaves gt2_sid zeroed. On this call adb-ntsc.prg wrote
     * a stale register image, and the SID played a short tone. */
    if (restart)
    {
        song_restart();
        return;
    }
    SID_VOICE(0);
    SID_VOICE(1);
    SID_VOICE(2);
    /* $D415 stays 0: the player only ever clears the low cutoff bits. */
    gt2_sid[0x16] = p.sid_cutoff;
    gt2_sid[0x17] = p.sid_filtctrl;
    gt2_sid[0x18] = p.sid_filttype;
    filter_exec();
    for (c = 0; c < 3; ++c)
        chn_exec();
}

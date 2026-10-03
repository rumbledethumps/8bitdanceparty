/* SID to OPL2 mapper for the C64 tune.
 *
 * SID voice v plays on OPL2 channels 2v and 2v+1 with one F-number and one
 * envelope. Channel 2v (carrier x1, modulator x2) has only odd harmonics.
 * Channel 2v+1 (carrier x2, modulator x6) has only the even harmonics that
 * are not multiples of 6. Neither ratio puts a sideband at 0 Hz, so neither
 * channel has a DC offset. A pulse of duty d splits into an odd part of
 * power d/2 and an even part of power d(1-2d)/2, so the pulse width sets the
 * two carrier levels, and the two spectra never overlap whatever the phase
 * between the channels. The modulator levels set the brightness from the
 * waveform and from the filter cutoff relative to the note.
 *
 * Channels 7 and 8 are feedback-noise sources for voices 2 and 3. Voice 1 of
 * the tune never uses the noise waveform, so channel 6 is a kick drum layer
 * under the drum-bass instrument.
 *
 * The SID envelope rate counter is 15 bits. A rate change that leaves the
 * counter above the new period wraps the counter, which stops the envelope
 * for up to 2 frames. The GoatTracker hard restart causes this on every
 * note: the hard-restart ADSR stops the envelope through the 2 gate-off
 * frames, and because SR is written 28 cycles before the gate, the next
 * attack can also start 2 frames late. Between the end of the first wrap and
 * the gate, the release runs at rate 0 for about 70 cycles plus the counter
 * value at the hard restart, so after a slow rate the level at the gate can
 * be anything from the held level down to silence.
 */
#include "opl.h"
#include "sid2opl.h"

/* odd and even layer attenuation by folded pulse width / 32 */
static const uint8_t pw_att_a[65] = {
    28, 24, 20, 18, 16, 15, 14, 13, 12, 11, 11, 10, 10, 9, 9, 8,
    8, 8, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4,
    4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
    2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0,
};
static const uint8_t pw_att_b[65] = {
    28, 24, 20, 18, 16, 15, 14, 13, 13, 12, 12, 11, 11, 11, 10, 10,
    10, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 15, 16, 18, 20, 24,
    63,
};
/* SID ADSR nibbles to OPL AR and SL */
static const uint8_t atk_hi[16] = {
    192, 160, 128, 128, 112, 112, 112, 96, 96, 80, 64, 48, 48, 16, 16, 16,
};
static const uint8_t sus_hi[16] = {
    240, 128, 96, 80, 64, 48, 48, 32, 32, 16, 16, 16, 16, 0, 0, 0,
};
/* SID decay or release nibble to OPL DR or RR, one row for each block / 2,
 * because the OPL adds block / 2 quarter steps to every rate even with KSR
 * off */
static const uint8_t rate[64] = {
    11, 11, 10, 9, 8, 8, 8, 7, 7, 6, 5, 4, 4, 2, 1, 1,
    11, 10, 9, 9, 8, 7, 7, 7, 7, 5, 4, 4, 3, 2, 1, 1,
    11, 10, 9, 8, 8, 7, 7, 7, 6, 5, 4, 3, 3, 1, 1, 1,
    11, 10, 9, 8, 8, 7, 7, 6, 6, 5, 4, 3, 3, 1, 1, 1,
};
/* mean fall of the held level in TL steps during the hard-restart release,
 * by the SID rate before the hard restart */
static const uint8_t drop[16] = {
    1, 1, 1, 1, 1, 1, 2, 2, 2, 5, 8, 11, 16, 48, 63, 63,
};
/* SID attack 12 and 13: counter advance per frame in 1/65536 of the period */
static const uint8_t ph_lo[2] = {242, 174};
static const uint8_t ph_hi[2] = {51, 102};
/* SID levels where the release step time doubles */
static const uint8_t exp_lvl[6] = {93, 54, 26, 14, 6, 0};
/* cutoff $D416 / 2 to log2 Hz in 1/8 octave, between the 6581 and 8580 curves */
static const uint8_t fc_log[128] = {
    57, 63, 66, 68, 70, 71, 72, 73, 74, 74, 75, 76, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 87, 88, 89, 90, 91, 91, 92, 92,
    93, 93, 94, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 100, 100, 101,
    101, 101, 102, 102, 102, 103, 103, 103, 104, 104, 104, 104, 104, 105, 105, 105,
    105, 105, 106, 106, 106, 106, 106, 107, 107, 107, 107, 108, 108, 108, 108, 109,
    109, 109, 109, 109, 109, 109, 109, 110, 110, 110, 110, 110, 110, 110, 110, 110,
    111, 111, 111, 111, 111, 111, 111, 111, 112, 112, 112, 112, 112, 112, 112, 112,
    112, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 114, 114, 114,
};
/* log2 of the frequency mantissa in 1/8 octave */
static const uint8_t mant_log[16] = {
    0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8,
};
/* low-pass modulator and carrier attenuation by log2(cutoff / note), -2..+6 octaves in 1/4 octave */
static const uint8_t flt_mod[32] = {
    7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 5, 4, 4,
    3, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};
static const uint8_t flt_att[32] = {
    21, 19, 16, 13, 10, 7, 4, 2, 2, 2, 2, 2, 2, 3, 3, 3,
    3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
};
/* Attenuation by GT2 instrument, set so that the loudness of each
 * instrument in the mix matches the mean of the 6581 and the 8580. The
 * filter resonance, which the mapping leaves out, is part of these values. */
static const uint8_t ins_tl[16] = {
    3, 5, 4, 4, 5, 0, 0, 0, 8, 4, 3, 6, 4, 3, 4, 3,
};
/* $D418 volume to attenuation */
static const uint8_t vol_att[16] = {
    63, 31, 23, 19, 15, 13, 11, 9, 7, 6, 5, 4, 3, 2, 1, 0,
};

/* levels in OPL TL steps of 0.75 dB; MA_ and MB_ are modulator levels */
#define MUTE 63
#define MA_PULSE 27
#define MB_PULSE 34
#define MA_SAW 25
#define MB_SAW 32
#define SAW_A 8
#define SAW_B 14
#define MA_TRI 43
#define TRI_A 9
#define NOISE_ATT 10
/* the filter resonance makes low-passed noise louder on both SIDs */
#define NOISE_LP 3
#define COMBO_ATT 4
#define FLT_NONE 1
#define FLT_HP 4
/* Noise channel F-number and block, and the rate table row of the block.
 * With F-number 512 the feedback noise repeats every 512 samples, a 97 Hz
 * buzz in long tails; with 513 the period is 32768 samples. */
#define NOISE_B0 0x16
#define NOISE_A0 0x01
#define NOISE_ROF 0x20

/* GT2 instrument of the drum-bass, and the kick layer keyed frames and level */
#define KICK_INSTR 4
#define KICK_FRAMES 3
#define KICK_TL 10

/* operator offsets: odd layer, even layer, noise; voice-major */
static const uint8_t op_tab[9] = {0x00, 0x01, 0x10, 0x02, 0x08, 0x11, 0x09, 0x0A, 0x12};
static const uint8_t init_reg[5] = {0x20, 0x23, 0x40, 0x43, 0x60};
static const uint8_t init_val[15] = {
    0x22, 0x21, MA_PULSE, MUTE, 0xF0,
    0x26, 0x22, MB_PULSE, MUTE, 0xF0,
    0x21, 0x21, MUTE, MUTE, 0xF0,
};
/* register and value pairs: the kick layer, a sine with a short FM click
 * that falls from 147 Hz to 44 Hz, then the noise channel F-numbers */
static const uint8_t init_pairs[22] = {
    0xC6, 0x00, 0x30, 0x01, 0x50, 0x24, 0x70, 0xF9, 0x90, 0xFF,
    0x33, 0x01, 0x53, KICK_TL, 0x73, 0xFA, 0x93, 0xFA,
    0xA7, NOISE_A0, 0xA8, NOISE_A0,
};
static const uint8_t kick_a0[KICK_FRAMES + 1] = {0x07, 0x05, 0x8E, 0xA0};
static const uint8_t kick_b0[KICK_FRAMES + 1] = {0x0B, 0x0A, 0x06, 0x03};
static const uint8_t bit_v[3] = {1, 2, 4};
/* The OPL attack is exponential and cannot reproduce a linear SID attack of
 * 250 ms or longer, so those notes start at full OPL level with the decay
 * stopped while the carrier attenuation ramps. Level step per frame (full
 * level 256) for attack 9..15, whole part and 1/256 part. */
static const uint8_t sw_whole[7] = {17, 8, 5, 4, 1, 0, 0};
static const uint8_t sw_frac[7] = {0, 128, 80, 64, 107, 218, 136};

/* shadow of OPL2 registers 0x00-0xC8; reg is built with separate 8-bit
 * adds, which cc65 compiles without the int promotion of a sum */
static uint8_t shadow[0xC9];

/* per-voice state */
static uint8_t vs[25][3];
#define last_flo vs[0]
#define last_fhi vs[1]
#define last_ad vs[2]
#define last_sr vs[3]
#define gate vs[4]
#define started vs[5]
#define keyon vs[6]
#define freeze vs[7]
#define held vs[8]
#define ract vs[9]
#define fn_a0 vs[10]
#define fn_b0 vs[11]
#define f_log vs[12]
#define swon vs[13]
#define swatt vs[14]
#define swl vs[15]
#define swf vs[16]
#define sus vs[17]
#define relf vs[18]
#define rof vs[19]
#define hdrop vs[20]
#define aph_l vs[21]
#define aph_h vs[22]
#define sw0 vs[23]
#define glev vs[24]

/* the most used scalars in the cc65 zero page */
#ifdef __CC65__
#pragma bss-name (push, "ZEROPAGE")
#endif
static const uint8_t *s;
static uint8_t v, j, c, k, i, n, e, reg, val;
static uint16_t x;
#ifdef __CC65__
#pragma bss-name (pop)
#pragma zpsym ("s")
#pragma zpsym ("v")
#pragma zpsym ("j")
#pragma zpsym ("c")
#pragma zpsym ("k")
#pragma zpsym ("i")
#pragma zpsym ("n")
#pragma zpsym ("e")
#pragma zpsym ("reg")
#pragma zpsym ("val")
#pragma zpsym ("x")
#endif
static const uint8_t *ins;
static uint8_t g, nop, ctrl, ad, sr, pwl, pwh, retrig, tdrop, kt;
static uint8_t op[3], mods[3], tls[3];
static uint8_t fc, mode, route, lvl, al;
static int8_t blk;

static void put(void)
{
    if (shadow[reg] != val)
    {
        shadow[reg] = val;
        opl_write(reg, val);
    }
}

static void pitch(void)
{
    if (!(c | k))
    {
        fn_a0[v] = 0;
        fn_b0[v] = 0;
        f_log[v] = 0;
        return;
    }
    n = 0;
    if (k)
        x = ((uint16_t)k << 8) | c;
    else
    {
        x = (uint16_t)c << 8;
        n = 8;
    }
    while (!(x & 0x8000))
    {
        x <<= 1;
        ++n;
    }
    i = (uint8_t)(x >> 8);
    /* F-number x32 at block 6 is x * 985248 / 49716 / 32 = x * 0.619328 */
    x = (x >> 1) + (x >> 3) - i - (i >> 1) + (i >> 4) - (i >> 6);
    blk = 6 - n;
    if (x >= 32752)
    {
        x = (x + 32) >> 6;
        ++blk;
    }
    else
        x = (x + 16) >> 5;
    if (blk < 0)
    {
        x >>= -blk;
        blk = 0;
    }
    fn_a0[v] = (uint8_t)x;
    fn_b0[v] = (uint8_t)(x >> 8) | (blk << 2);
    n = 120 - (n << 3) + mant_log[(i >> 3) & 15];
    f_log[v] = n > 33 ? n - 33 : 0;
}

/* operators of voice v; voice 1 has no noise channel */
static void ops(void)
{
    n = v + v + v;
    op[0] = op_tab[n];
    ++n;
    op[1] = op_tab[n];
    ++n;
    op[2] = op_tab[n];
    nop = v ? 3 : 2;
}

static void envelope(void)
{
    e = rof[v];
    for (j = 0; j < nop; ++j)
    {
        if (j != 1)
        {
            if (j)
                e = NOISE_ROF;
            c = held[v] & 1 ? 0 : held[v] ? 0xF0 : atk_hi[ad >> 4] | rate[e | (ad & 15)];
            k = sus_hi[sr >> 4] | (held[v] & 1 ? 0 : rate[e | (sr & 15)]);
        }
        reg = op[j];
        reg += 0x63;
        val = c;
        put();
        reg += 0x20;
        val = k;
        put();
    }
}

void sid2opl_reset(void)
{
    for (i = 0; i < sizeof shadow; ++i)
        shadow[i] = 0;
    for (i = 0; i < sizeof vs; ++i)
        (&vs[0][0])[i] = 0;
    reg = 0x01;
    val = 0x20;
    put();
    for (v = 0; v < 9; ++v)
    {
        reg = 0xC0 + v;
        val = 0x0E;
        put();
    }
    ad = 0;
    sr = 0;
    for (v = 0; v < 3; ++v)
    {
        ops();
        k = 0;
        for (j = 0; j < nop; ++j)
            for (i = 0; i < 5; ++i)
            {
                reg = init_reg[i] + op[j];
                val = init_val[k++];
                put();
            }
        envelope();
    }
    for (i = 0; i < sizeof init_pairs; i += 2)
    {
        reg = init_pairs[i];
        val = init_pairs[i + 1];
        put();
    }
    kt = KICK_FRAMES + 1;
}

/* The drop table holds a mean over the counter value, which is a poor
 * estimate when the counter period is more than half of the 6800 cycles that
 * the rate-0 release takes from full level to silence. During a swell the
 * counter value follows from the frames since the gate, so after a swell at
 * attack 12 or 13 the level at the gate is the swell level less the rate-0
 * release over the counter value plus about 70 cycles, the 8 steps added to
 * x. x counts release steps of 9 cycles at first, then of the step time of
 * each level range. An attack that rose past the lower end of a range leaves
 * the step time of the range below in place for the first range. */
static void hfall(void)
{
    k = ract[v] - 12;
    c = swl[v];
    if (k < 2 && gate[v] && ract[v] == last_ad[v] >> 4 && c)
    {
        x = aph_h[v];
        if (k)
            x = (x << 2) + x;
        else
            x = (x << 1) - (x >> 2);
        x += 8;
        for (j = 0; c <= exp_lvl[j]; ++j)
            x >>= 1;
        k = sw0[v] <= exp_lvl[j];
        if (k)
            x >>= 1;
        for (;;)
        {
            e = c - exp_lvl[j];
            if (x < e)
            {
                c -= (uint8_t)x;
                break;
            }
            x -= e;
            c = exp_lvl[j];
            if (!c)
                break;
            ++j;
            if (!k)
                x >>= 1;
            k = 0;
        }
        glev[v] = c;
        c = (pw_att_a[c >> 2] - pw_att_a[swl[v] >> 2]) << 1;
    }
    else
    {
        glev[v] = 0xFF;
        c = drop[ract[v]];
    }
    hdrop[v] = c;
}

static void voice(void)
{
    ops();
    al = lvl + ins_tl[ins[v] & 15];
    c = *s++;
    k = *s++;
    pwl = *s++;
    pwh = *s++;
    ctrl = *s++;
    ad = *s++;
    sr = *s++;

    if (c != last_flo[v] || k != last_fhi[v])
    {
        last_flo[v] = c;
        last_fhi[v] = k;
        pitch();
    }

    /* gate, attack phase and the rate counter wrap */
    retrig = 0;
    tdrop = 0;
    g = ctrl & 1;
    if (g)
    {
        if (!gate[v])
        {
            if (ins[v] == KICK_INSTR && (ctrl & 0x08))
                kt = 0;
            started[v] = 0;
            swon[v] = ad >= 0x90;
            /* the ramp starts from the previous sustain level, about 12 dB
             * lower for each frame of release that ran without a hold, or
             * from the level after a computed hard-restart fall */
            c = (sus[v] << 4) + sus[v];
            for (k = relf[v]; k; --k)
                c >>= 2;
            if (glev[v] != 0xFF)
                c = glev[v];
            glev[v] = 0xFF;
            swl[v] = c;
            sw0[v] = c;
            swf[v] = 0;
            aph_l[v] = 0;
            aph_h[v] = 0;
            tdrop = hdrop[v];
            hdrop[v] = 0;
            c = swatt[v] + tdrop;
            swatt[v] = c > MUTE ? MUTE : c;
        }
        if (swon[v])
            i = ad >> 4;
        else
            i = started[v] ? ad & 15 : ad >> 4;
    }
    else
        i = sr & 15;
    n = freeze[v];
    /* AD and SR are written 14 and 28 cycles before the gate, so a slower
     * rate written there lets the counter pass a period of 9 */
    if (!n && (i < ract[v] || (!i && g != gate[v] && ((g ? sr : ad) & 15) > ract[v])))
    {
        n = ract[v] < 14 ? 2 : 1;
        if (!g)
            hfall();
    }
    if (!g)
    {
        keyon[v] = 0;
        swon[v] = 0;
        if (gate[v])
            relf[v] = 0;
        if (!n && relf[v] < 3)
            ++relf[v];
    }
    else
    {
        if (!started[v] && !(ctrl & 0x08) && (ctrl & 0xF0))
        {
            retrig = keyon[v];
            keyon[v] = 1;
            started[v] = 1;
        }
        if (swon[v])
        {
            c = swl[v];
            if (!n)
            {
                j = (ad >> 4) - 9;
                k = swf[v] + sw_frac[j];
                c += sw_whole[j] + (k < swf[v]);
                swf[v] = k;
                /* swl is the level for hfall() in the frame after the attack
                 * ends */
                if (c < swl[v])
                {
                    swon[v] = 0;
                    c = 255;
                }
                swl[v] = c;
                j -= 3;
                if (j < 2)
                {
                    e = aph_l[v] + ph_lo[j];
                    aph_h[v] += ph_hi[j] + (e < aph_l[v]);
                    aph_l[v] = e;
                }
            }
            k = pw_att_a[c >> 2] << 1;
            swatt[v] = k > MUTE ? MUTE : k;
        }
        if (!swon[v] && !n)
            swatt[v] = 0;
    }
    ract[v] = i;
    gate[v] = g;
    if (g && started[v])
        sus[v] = sr >> 4;
    k = (n ? 1 : 0) | (swon[v] ? 2 : 0);
    /* rate table row of the block */
    i = (fn_b0[v] & 0x18) << 1;
    if (k != held[v] || ad != last_ad[v] || sr != last_sr[v] || i != rof[v])
    {
        held[v] = k;
        rof[v] = i;
        last_ad[v] = ad;
        last_sr[v] = sr;
        envelope();
    }
    freeze[v] = n ? n - 1 : 0;

    /* waveform, pulse width and filter to levels and brightness */
    mods[2] = MUTE;
    tls[2] = MUTE;
    n = !(ctrl & 0x08) && (ctrl & 0xF0);
    if (n)
    {
        reg = op[0];
        reg += 0x40;
        mods[0] = shadow[reg];
        reg = op[1];
        reg += 0x40;
        mods[1] = shadow[reg];
        tls[0] = MUTE;
        tls[1] = MUTE;
        if (ctrl & 0x80)
        {
            mods[2] = 0;
            tls[2] = al + swatt[v];
            tls[2] += NOISE_ATT;
            if ((route & bit_v[v]) && (mode & 0x10))
                tls[2] -= NOISE_LP;
        }
        else
        {
            if (ctrl & 0x40)
            {
                pwh &= 0x0F;
                if (pwh & 0x08)
                {
                    pwh = 0x0F - pwh;
                    pwl = 0xFF - pwl;
                    if (!++pwl)
                        ++pwh;
                }
                i = (pwh & 0x08) ? 64 : (uint8_t)(pwh << 3) | (pwl >> 5);
                tls[0] = pw_att_a[i];
                tls[1] = pw_att_b[i];
                mods[0] = MA_PULSE;
                mods[1] = MB_PULSE;
                if (ctrl & 0x30)
                {
                    tls[0] += COMBO_ATT;
                    tls[1] += COMBO_ATT;
                }
            }
            else if (ctrl & 0x20)
            {
                tls[0] = SAW_A;
                tls[1] = SAW_B;
                mods[0] = MA_SAW;
                mods[1] = MB_SAW;
                if (ctrl & 0x10)
                {
                    tls[0] += COMBO_ATT;
                    tls[1] += COMBO_ATT;
                }
            }
            else
            {
                tls[0] = TRI_A;
                mods[0] = MA_TRI;
            }
            i = FLT_NONE;
            if (route & bit_v[v])
            {
                if (!(mode & 0x70))
                    i = MUTE;
                else if (mode & 0x10)
                {
                    c = fc_log[fc >> 1];
                    c += 16;
                    k = f_log[v];
                    if (c > k)
                    {
                        c -= k;
                        c >>= 1;
                    }
                    else
                        c = 0;
                    if (c > 31)
                        c = 31;
                    mods[0] += flt_mod[c];
                    mods[1] += flt_mod[c];
                    i = flt_att[c];
                }
                else
                    i = FLT_HP;
            }
            if (mods[0] > MUTE)
                mods[0] = MUTE;
            if (mods[1] > MUTE)
                mods[1] = MUTE;
            tls[0] += i + al + swatt[v];
            tls[1] += i + al + swatt[v];
        }
    }
    /* TEST or no waveform: a zero F-number stops the phase, which leaves a
     * DC level like the SID output in those frames */
    c = n ? 0 : 2;
    /* the loop below skips the carriers in TEST frames, so the hard-restart
     * fall is written to them here */
    if (!n && tdrop)
    {
        for (c = 0; c < 2; ++c)
        {
            reg = op[c];
            reg += 0x40;
            mods[c] = shadow[reg];
            reg += 3;
            tls[c] = shadow[reg] + tdrop;
        }
        c = 0;
    }
    for (j = c; j < nop; ++j)
    {
        reg = op[j];
        reg += 0x40;
        val = mods[j];
        put();
        reg += 3;
        val = tls[j] > MUTE ? MUTE : tls[j];
        put();
    }

    /* frequency and key */
    c = v + v;
    k = keyon[v] ? 0x20 : 0;
    i = n ? fn_a0[v] : 0;
    n = n ? fn_b0[v] : 0;
    if (retrig)
    {
        reg = c;
        reg += 0xB0;
        val = shadow[reg] & 0x1F;
        put();
        ++reg;
        val = shadow[reg] & 0x1F;
        put();
        if (v)
        {
            reg = v;
            reg += 0xB6;
            val = shadow[reg] & 0x1F;
            put();
        }
    }
    reg = c;
    reg += 0xA0;
    val = i;
    put();
    ++reg;
    put();
    reg = c;
    reg += 0xB0;
    val = n | k;
    put();
    ++reg;
    put();
    if (v)
    {
        reg = v;
        reg += 0xB6;
        val = (mods[2] ? 0 : NOISE_B0) | k;
        put();
    }
}

/* the key-off before the key-on restarts the kick envelope when the
 * previous kick is still keyed */
static void kick(void)
{
    if (kt > KICK_FRAMES)
        return;
    reg = 0xB6;
    if (!kt)
    {
        val = shadow[reg] & 0x1F;
        put();
        reg = 0x53;
        val = KICK_TL + lvl;
        put();
    }
    reg = 0xA6;
    val = kick_a0[kt];
    put();
    reg = 0xB6;
    val = kick_b0[kt];
    if (kt < KICK_FRAMES)
        val |= 0x20;
    put();
    ++kt;
}

void sid2opl_update(const uint8_t *sid, const uint8_t *instr)
{
    s = sid;
    ins = instr;
    fc = s[22];
    route = s[23];
    mode = s[24];
    lvl = vol_att[mode & 15];
    for (v = 0; v < 3; ++v)
        voice();
    kick();
}

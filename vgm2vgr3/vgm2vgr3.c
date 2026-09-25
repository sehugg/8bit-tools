/* vgm2vgr3.c -- VGM -> VGR3 encoder (see new.md, vgr3_format.h).
 *
 * 1. Parse the VGM into a per-frame register file: for each frame and
 *    byte, the last value written and whether any write that frame had
 *    a side effect (must be replayed even if the value didn't change).
 * 2. Group bytes into channels (per chip, with a combined or split
 *    layout chosen per voice by trial encoding). Each channel becomes a
 *    list of tokens: one SET per frame that changes something (only
 *    changed or side-effect bytes), carrying the wait to its next event.
 * 3. Greedy parse over all channels' tokens: at each item, find the
 *    earlier run of whole items (any channel of the same width) that
 *    saves the most bytes as a CALL. Runs may contain CALLs, up to
 *    VGR3_MAX_DEPTH. Then pick a dictionary from the literal SETs left
 *    over and parse again.
 * 4. Emit, then decode the emitted file with vgr3_play.c and compare
 *    every frame against the source. A file that fails is not written.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vgm_format.h"
#include "vgr3_format.h"
#include "vgr3_play.h"

static void *xrealloc(void *p, size_t n) {
    void *r = realloc(p, n ? n : 1);
    if (!r) { fprintf(stderr, "out of memory\n"); exit(1); }
    return r;
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct { uint8_t *data; size_t len, cap; } ByteBuf;
static void bbPut(ByteBuf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = (b->len + n) * 2 + 256;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}
static void bbU8(ByteBuf *b, uint8_t v) { bbPut(b, &v, 1); }
static void bbU16(ByteBuf *b, uint16_t v) { bbU8(b, (uint8_t)v); bbU8(b, (uint8_t)(v >> 8)); }
static void bbU32(ByteBuf *b, uint32_t v) { bbU16(b, (uint16_t)v); bbU16(b, (uint16_t)(v >> 16)); }

/* ---------------------------------------------------------------- */
/* options                                                           */

static int g_tickRate = 60;
static int g_maxDepth = VGR3_MAX_DEPTH;
static int g_dictMax = VGR3_DICT_MAX;
static int g_window = 1024;     /* candidates tried per position */
static int g_cross = 1;         /* CALLs into other channels of the same width */
static int g_farPenalty = 0;    /* parse bias against CALLs that can't use CALLS */
static int g_lookahead = 1;     /* parse picks each CALL with one step of lookahead */
static int g_dictCalls = 1;     /* dict entries may be CALLs */
static int g_windowSet, g_crossSet, g_farPenaltySet;
static int g_layout = -1;       /* -1 = choose per voice, else force option */
static int g_verbose = 0;
static int g_quiet = 0;         /* no warnings during trial encodes */
static const char *g_samplesPath = NULL;   /* --samples: write DPCM bank here */
static ByteBuf g_sampleBank;               /* NES DPCM data blocks, in file order */
static int g_sampleBlocks = 0;

/* ---------------------------------------------------------------- */
/* chips                                                             */

typedef struct { uint8_t base, w; uint32_t owned; } ChanDef;
typedef struct { int nch; ChanDef ch[3]; } Layout;
typedef struct { const char *name; int nopt; Layout opt[3]; } VoiceDef;

#define L1(a) { 1, { a } }
#define L2(a, b) { 2, { a, b } }
#define L3(a, b, c) { 3, { a, b, c } }
#define C(b, w, o) { b, w, o }

static const VoiceDef kSnVoices[] = {
    { "T0", 2, { L1(C(0, 3, 7)), L2(C(0, 2, 3), C(2, 1, 1)) } },
    { "T1", 2, { L1(C(3, 3, 7)), L2(C(3, 2, 3), C(5, 1, 1)) } },
    { "T2", 2, { L1(C(6, 3, 7)), L2(C(6, 2, 3), C(8, 1, 1)) } },
    { "NOI", 2, { L1(C(9, 2, 3)), L2(C(9, 1, 1), C(10, 1, 1)) } },
};
static const VoiceDef kNesVoices[] = {
    { "P1", 2, { L1(C(0x0, 4, 0xF)), L2(C(0x0, 1, 1), C(0x1, 3, 7)) } },
    { "P2", 2, { L1(C(0x4, 4, 0xF)), L2(C(0x4, 1, 1), C(0x5, 3, 7)) } },
    { "TRI", 2, { L1(C(0x8, 4, 0xD)), L2(C(0x8, 1, 1), C(0xA, 2, 3)) } },
    { "NOI", 2, { L1(C(0xC, 4, 0xD)), L2(C(0xC, 1, 1), C(0xE, 2, 3)) } },
    { "DMC", 1, { L1(C(0x10, 4, 0xF)) } },
    { "CTL", 1, { L1(C(0x15, 3, 0x5)) } },
};
/* GB: the volume/envelope byte sits in the middle of each voice, so the
 * two-way split leaves a hole in the first channel's window. */
static const VoiceDef kGbVoices[] = {
    { "CH1", 3, { L1(C(0x00, 5, 0x1F)), L2(C(0x00, 5, 0x1B), C(0x02, 1, 1)),
                  L3(C(0x00, 2, 3), C(0x02, 1, 1), C(0x03, 2, 3)) } },
    { "CH2", 3, { L1(C(0x06, 4, 0xF)), L2(C(0x06, 4, 0xD), C(0x07, 1, 1)),
                  L3(C(0x06, 1, 1), C(0x07, 1, 1), C(0x08, 2, 3)) } },
    { "CH3", 3, { L1(C(0x0A, 5, 0x1F)), L2(C(0x0A, 5, 0x1B), C(0x0C, 1, 1)),
                  L3(C(0x0A, 2, 3), C(0x0C, 1, 1), C(0x0D, 2, 3)) } },
    { "CH4", 3, { L1(C(0x10, 4, 0xF)), L2(C(0x10, 4, 0xD), C(0x11, 1, 1)),
                  L3(C(0x10, 1, 1), C(0x11, 1, 1), C(0x12, 2, 3)) } },
    { "CTL", 1, { L1(C(0x14, 3, 7)) } },
    { "WAVE", 1, { L1(C(0x20, 16, 0xFFFF)) } },
};
static const VoiceDef kAyVoices[] = {
    { "TA", 1, { L1(C(0, 2, 3)) } },
    { "TB", 1, { L1(C(2, 2, 3)) } },
    { "TC", 1, { L1(C(4, 2, 3)) } },
    { "NMIX", 1, { L1(C(6, 2, 3)) } },
    { "VOL", 2, { L1(C(8, 3, 7)), L3(C(8, 1, 1), C(9, 1, 1), C(10, 1, 1)) } },
    { "ENV", 1, { L1(C(11, 3, 7)) } },
};
/* POKEY: four channels, each an AUDF/AUDC pair, plus AUDCTL/STIMER/
 * SKRES. The POT registers (0x0B-0x0F) have no audio effect and are
 * dropped (see pokeyWrite). */
static const VoiceDef kPokeyVoices[] = {
    { "V1", 2, { L1(C(0, 2, 3)), L2(C(0, 1, 1), C(1, 1, 1)) } },
    { "V2", 2, { L1(C(2, 2, 3)), L2(C(2, 1, 1), C(3, 1, 1)) } },
    { "V3", 2, { L1(C(4, 2, 3)), L2(C(4, 1, 1), C(5, 1, 1)) } },
    { "V4", 2, { L1(C(6, 2, 3)), L2(C(6, 1, 1), C(7, 1, 1)) } },
    { "CTL", 1, { L1(C(8, 3, 7)) } },
};
/* SID: three voices, each freq(2)/PW(2)/control/AD/SR, plus the shared
 * filter/volume block. A voice can split into frequency and "rest" so a
 * pitch ramp doesn't drag a constant PW/ADSR along with it. */
static const VoiceDef kSidVoices[] = {
    { "V1", 2, { L1(C(0x00, 7, 0x7F)), L2(C(0x00, 2, 3), C(0x02, 5, 0x1F)) } },
    { "V2", 2, { L1(C(0x07, 7, 0x7F)), L2(C(0x07, 2, 3), C(0x09, 5, 0x1F)) } },
    { "V3", 2, { L1(C(0x0E, 7, 0x7F)), L2(C(0x0E, 2, 3), C(0x10, 5, 0x1F)) } },
    { "FLT", 2, { L1(C(0x15, 4, 0xF)), L2(C(0x15, 2, 3), C(0x17, 2, 3)) } },
};

static int g_chip;
static int g_nregs;
static const VoiceDef *g_voices;
static int g_nvoices;

/* Bytes whose writes can have side effects (always replayed when the
 * source writes them, never written as filler). */
static int seCapable(int a) {
    switch (g_chip) {
    case VGR3_CHIP_SN76489: return a == 9;
    case VGR3_CHIP_AY8910: return a == 13;
    case VGR3_CHIP_POKEY: return a == 0x09 || a == 0x0A;   /* STIMER, SKRES are commands */
    case VGR3_CHIP_NES_APU: return a == 0x3 || a == 0x7 || a == 0xB || a == 0xF || a == 0x15 || a == 0x17;
    case VGR3_CHIP_SID: return a == 0x04 || a == 0x0B || a == 0x12;   /* control: gate write */
    case VGR3_CHIP_GB_DMG:
        return a == 0x4 || a == 0x9 || a == 0xE || a == 0x13 ||   /* NRx4 trigger */
               a == 0x1 || a == 0x6 || a == 0xB || a == 0x10;     /* NRx1 length load */
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* VGM -> per-frame register file                                     */

static uint32_t g_nframes, g_cap;
static uint8_t *g_wr;       /* [f*nregs+a]: bit0 written, bit1 side effect */
static uint8_t *g_val;      /* last value written in frame f */
static int16_t *g_state;    /* value after frame f, -1 = never written */
static int16_t g_cur[VGR3_MAX_REGS];
static uint32_t g_loopFrame = VGR3_LOOP_NONE;
static uint32_t g_elapsed, g_spt;

static void regWrite(int a, uint8_t v, int se) {
    uint32_t f = g_elapsed / g_spt;
    if (f >= g_cap) {
        uint32_t nc = f * 2 + 1024;
        g_wr = xrealloc(g_wr, (size_t)nc * g_nregs);
        g_val = xrealloc(g_val, (size_t)nc * g_nregs);
        memset(g_wr + (size_t)g_cap * g_nregs, 0, (size_t)(nc - g_cap) * g_nregs);
        g_cap = nc;
    }
    if (f + 1 > g_nframes) g_nframes = f + 1;
    size_t i = (size_t)f * g_nregs + a;
    g_wr[i] |= (uint8_t)(1 | (se ? 2 : 0));
    g_val[i] = v;
    g_cur[a] = v;
}

static int g_snLatch = -1;   /* register-file byte the next data byte hits */

static void snWrite(uint8_t b) {
    if (b & 0x80) {
        int cc = (b >> 5) & 3, t = (b >> 4) & 1;
        if (t) {
            g_snLatch = cc < 3 ? cc * 3 + 2 : 10;
            regWrite(g_snLatch, b & 0x0F, 0);
        } else if (cc < 3) {
            g_snLatch = cc * 3 + 1;
            regWrite(cc * 3, b & 0x0F, 0);
        } else {
            g_snLatch = 9;
            regWrite(9, b & 0x07, 1);
        }
    } else if (g_snLatch >= 0) {
        int a = g_snLatch;
        if (a % 3 == 1 && a < 9) regWrite(a, b & 0x3F, 0);
        else regWrite(a, a == 9 ? (b & 0x07) : (b & 0x0F), a == 9);
    }
}

static void nesWrite(uint8_t a, uint8_t v) {
    int se;
    if (a > 0x17 || a == 0x09 || a == 0x0D || a == 0x14 || a == 0x16) return;
    switch (a) {
    case 0x0B: se = !(g_cur[0x08] & 0x80); break;              /* linear counter reload */
    case 0x0F: se = (g_cur[0x0C] & 0x30) != 0x30; break;       /* inaudible if halted + const vol */
    default: se = seCapable(a);
    }
    regWrite(a, v, se);
}

static void gbWrite(uint8_t a, uint8_t v) {
    if (a > 0x2F || (a > 0x16 && a < 0x20)) return;
    regWrite(a, v, (a == 0x4 || a == 0x9 || a == 0xE || a == 0x13) && (v & 0x80));
}

static void ayWrite(uint8_t a, uint8_t v) {
    if (a & 0x80 || a > 13) return;   /* second chip / IO ports */
    regWrite(a, v, a == 13);
}

static void pokeyWrite(uint8_t a, uint8_t v) {
    /* Only the audio registers are modelled. POT0-3/POTGO (0x0B-0x0F)
     * don't affect sound, so writes there are dropped. STIMER (0x09)
     * and SKRES (0x0A) are command registers: any write triggers the
     * reset regardless of value, so they always replay. */
    if (a > 0x0A) return;
    regWrite(a, v, a == 0x09 || a == 0x0A);
}

/* DefleMask's SID VGM arrives as YM2151 0xB6 writes whose "register"
 * is really a SID register ($D400+aa). The write stream already has the
 * SID register layout, so it maps 1:1 into the shadow file. */
static void sidWrite(uint8_t a, uint8_t v) {
    if (a > VGM_YM2151_MAX_SID_REG) return;   /* read-only POT/OSC/ENV */
    regWrite(a, v, seCapable(a));
}

static int parseVgm(const uint8_t *file, size_t size) {
    if (size < VGM_HDR_MIN_READ_SIZE || memcmp(file, VGM_MAGIC_STR, 4) != 0) {
        fprintf(stderr, "not a VGM file\n");
        return 0;
    }
    uint32_t ver = rd32(file + VGM_HDR_OFF_VERSION);
    uint32_t dofs = ver >= VGM_VERSION_1_50 ? rd32(file + VGM_HDR_OFF_VGM_DATA_OFFSET) : 0;
    /* Where the command stream begins. Files exist in the wild that
     * report a v1.5x version but still carry only the classic 0x40-byte
     * header (e.g. the circus samples), so a clock field that lives past
     * the data offset is really stream data. Reading it would invent a
     * chip that isn't there, so gate every field on its offset. */
    uint32_t dataOff = dofs ? VGM_HDR_OFF_VGM_DATA_OFFSET + dofs : VGM_HDR_DEFAULT_DATA_OFFSET;
#define HDR_FIELD_PRESENT(off) ((uint32_t)(off) + 4 <= dataOff)
    uint32_t sn = HDR_FIELD_PRESENT(VGM_HDR_OFF_SN76489_CLOCK) ? rd32(file + VGM_HDR_OFF_SN76489_CLOCK) & 0x3FFFFFFFu : 0;
    uint32_t ay = ver >= VGM_VERSION_1_51 && HDR_FIELD_PRESENT(VGM_HDR_OFF_AY8910_CLOCK) ? rd32(file + VGM_HDR_OFF_AY8910_CLOCK) : 0;
    uint32_t nes = ver >= VGM_VERSION_1_60 && HDR_FIELD_PRESENT(VGM_HDR_OFF_NES_APU_CLOCK) ? rd32(file + VGM_HDR_OFF_NES_APU_CLOCK) : 0;
    uint32_t gb = ver >= VGM_VERSION_1_61 && HDR_FIELD_PRESENT(VGM_HDR_OFF_GB_DMG_CLOCK) ? rd32(file + VGM_HDR_OFF_GB_DMG_CLOCK) : 0;
    uint32_t pokey = ver >= VGM_VERSION_1_61 && HDR_FIELD_PRESENT(VGM_HDR_OFF_POKEY_CLOCK) ? rd32(file + VGM_HDR_OFF_POKEY_CLOCK) : 0;
    uint32_t ym2151 = HDR_FIELD_PRESENT(VGM_HDR_OFF_YM2151_CLOCK) ? rd32(file + VGM_HDR_OFF_YM2151_CLOCK) & 0x3FFFFFFFu : 0;
#undef HDR_FIELD_PRESENT
    uint32_t lofs = rd32(file + VGM_HDR_OFF_LOOP_OFFSET);
    size_t loopAbs = lofs ? VGM_HDR_OFF_LOOP_OFFSET + lofs : 0;
    uint32_t loopElapsed = 0;

    if (nes) { g_chip = VGR3_CHIP_NES_APU; g_nregs = VGR3_NREGS_NES_APU; g_voices = kNesVoices; g_nvoices = 6; }
    else if (gb) { g_chip = VGR3_CHIP_GB_DMG; g_nregs = VGR3_NREGS_GB_DMG; g_voices = kGbVoices; g_nvoices = 6; }
    else if (ay) { g_chip = VGR3_CHIP_AY8910; g_nregs = VGR3_NREGS_AY8910; g_voices = kAyVoices; g_nvoices = 6; }
    else if (pokey) { g_chip = VGR3_CHIP_POKEY; g_nregs = VGR3_NREGS_POKEY; g_voices = kPokeyVoices; g_nvoices = 5; }
    else if (sn) { g_chip = VGR3_CHIP_SN76489; g_nregs = VGR3_NREGS_SN76489; g_voices = kSnVoices; g_nvoices = 4; }
    /* No YM2151 support: a YM2151 clock in the header next to a 0xB6
     * stream of SID register numbers is DefleMask's SID export. A real
     * YM2151 write is caught below (register > 0x18). */
    else if (ym2151) { g_chip = VGR3_CHIP_SID; g_nregs = VGR3_NREGS_SID; g_voices = kSidVoices; g_nvoices = 4; }
    else { fprintf(stderr, "no supported chip\n"); return 0; }
    if ((nes != 0) + (gb != 0) + (ay != 0) + (sn != 0) + (pokey != 0) > 1)
        fprintf(stderr, "warning: several chips in file, encoding only chip type %d\n", g_chip);

    for (int a = 0; a < VGR3_MAX_REGS; a++) g_cur[a] = 0;
    g_spt = VGM_SAMPLES_PER_SEC / g_tickRate;
    const uint8_t *p = file + dataOff;
    const uint8_t *end = file + size;
    while (p < end) {
        if (loopAbs && (size_t)(p - file) == loopAbs) {
            g_loopFrame = g_elapsed / g_spt;
            loopElapsed = g_elapsed;
        }
        uint8_t op = *p++;
        if (op == VGM_CMD_END_OF_DATA) break;
        switch (op) {
        case VGM_CMD_SN76489_WRITE: if (g_chip == VGR3_CHIP_SN76489) snWrite(p[0]); p += 1; break;
        case VGM_CMD_AY8910_WRITE: if (g_chip == VGR3_CHIP_AY8910) ayWrite(p[0], p[1]); p += 2; break;
        case VGM_CMD_GB_DMG_WRITE: if (g_chip == VGR3_CHIP_GB_DMG) gbWrite(p[0], p[1]); p += 2; break;
        case VGM_CMD_NES_APU_WRITE: if (g_chip == VGR3_CHIP_NES_APU) nesWrite(p[0], p[1]); p += 2; break;
        case VGM_CMD_POKEY_WRITE: if (g_chip == VGR3_CHIP_POKEY) pokeyWrite(p[0], p[1]); p += 2; break;
        case VGM_CMD_YM2151_WRITE:
            if (g_chip == VGR3_CHIP_SID) {
                if (p[0] > VGM_YM2151_MAX_SID_REG) {
                    fprintf(stderr, "YM2151 register 0x%02X out of SID range; real YM2151 is not supported\n", p[0]);
                    return 0;
                }
                sidWrite(p[0], p[1]);
            }
            p += 2; break;
        case VGM_CMD_WAIT_NN: g_elapsed += rd16(p); p += 2; break;
        case VGM_CMD_WAIT_735: g_elapsed += 735; break;
        case VGM_CMD_WAIT_882: g_elapsed += 882; break;
        case VGM_CMD_DATA_BLOCK: {
            uint32_t bs = rd32(p + 2);
            /* Multiple type-0x07 blocks expand the NES DPCM bank, so
             * concatenate them in file order; a player loads the result
             * at $C000 to match the $4012/$4013 addressing. */
            if (g_samplesPath && p[0] == VGM_CMD_DATA_BLOCK_TAG &&
                p[1] == VGM_DB_NES_DPCM && p + 6 + bs <= end) {
                bbPut(&g_sampleBank, p + 6, bs);
                g_sampleBlocks++;
            }
            p += 6 + bs;
            break;
        }
        case VGM_CMD_PCM_RAM_WRITE: p += 11; break;
        case VGM_CMD_WAIT_OVERRIDE: p += 4; break;
        case VGM_CMD_GG_STEREO: p += 1; break;
        case VGM_CMD_DAC_SETUP: case VGM_CMD_DAC_SETDATA: case VGM_CMD_DAC_STARTFAST: p += 4; break;
        case VGM_CMD_DAC_SETFREQ: p += 5; break;
        case VGM_CMD_DAC_START: p += 10; break;
        case VGM_CMD_DAC_STOP: p += 1; break;
        default:
            if (op >= VGM_CMD_WAIT_N_PLUS1_LO && op <= VGM_CMD_WAIT_N_PLUS1_HI) g_elapsed += (op & 15) + 1;
            else if (op >= VGM_CMD_YM2612_DAC_LO && op <= VGM_CMD_YM2612_DAC_HI) g_elapsed += op & 15;
            else if (op >= VGM_RANGE_1BYTE_LO && op <= VGM_RANGE_1BYTE_HI) p += 1;
            else if (op >= VGM_RANGE_2BYTE_A_LO && op <= VGM_RANGE_2BYTE_A_HI) p += 2;
            else if (op >= VGM_RANGE_2BYTE_B_LO && op <= VGM_RANGE_2BYTE_B_HI) p += 2;
            else if (op >= VGM_RANGE_3BYTE_LO && op <= VGM_RANGE_3BYTE_HI) p += 3;
            else if (op >= VGM_RANGE_4BYTE_LO && op <= VGM_RANGE_4BYTE_HI) p += 4;
            else {
                fprintf(stderr, "unknown VGM opcode 0x%02X, stopping\n", op);
                p = end;
            }
        }
    }

    /* Song length: the loop period is what must come out right. */
    uint32_t total;
    if (g_loopFrame != VGR3_LOOP_NONE)
        total = g_loopFrame + (g_elapsed - loopElapsed + g_spt / 2) / g_spt;
    else
        total = (g_elapsed + g_spt / 2) / g_spt;
    if (total < g_nframes) total = g_nframes;
    if (total == 0) total = 1;
    if (g_loopFrame != VGR3_LOOP_NONE && g_loopFrame >= total) g_loopFrame = VGR3_LOOP_NONE;
    if (total > g_cap) {
        g_wr = xrealloc(g_wr, (size_t)total * g_nregs);
        g_val = xrealloc(g_val, (size_t)total * g_nregs);
        memset(g_wr + (size_t)g_cap * g_nregs, 0, (size_t)(total - g_cap) * g_nregs);
        g_cap = total;
    }
    g_nframes = total;

    g_state = xrealloc(NULL, sizeof(int16_t) * g_nframes * g_nregs);
    for (uint32_t f = 0; f < g_nframes; f++)
        for (int a = 0; a < g_nregs; a++) {
            size_t i = (size_t)f * g_nregs + a;
            g_state[i] = (g_wr[i] & 1) ? g_val[i] : f ? g_state[i - g_nregs] : -1;
        }

    /* GB NRx1 reloads the length counter on every write; that only
     * matters when length is enabled (NRx4 bit 6) around that frame. */
    if (g_chip == VGR3_CHIP_GB_DMG) {
        static const int nrx1[4] = { 0x1, 0x6, 0xB, 0x10 };
        for (uint32_t f = 0; f < g_nframes; f++)
            for (int c = 0; c < 4; c++) {
                size_t i = (size_t)f * g_nregs + nrx1[c];
                int16_t before = f ? g_state[i - g_nregs + 3] : 0, after = g_state[i + 3];
                if (before < 0) before = 0;
                if (after < 0) after = 0;
                if ((g_wr[i] & 1) && ((before | after) & 0x40)) g_wr[i] |= 2;
            }
    }
    return 1;
}

static int16_t stateAt(int32_t f, int a) { return f < 0 ? -1 : g_state[(size_t)f * g_nregs + a]; }

/* ---------------------------------------------------------------- */
/* tokens: one SET/LOAD/WAIT item, interned by (width, bytes)         */

typedef struct { uint8_t w, len, b[20]; int16_t dict; } Tok;
static Tok *g_toks;
static int g_ntoks, g_tokCap;
static int *g_tokHash;
static int g_tokHashSize;

static uint32_t tokHashOf(const Tok *t) {
    uint32_t h = 2166136261u ^ t->w;
    for (int i = 0; i < t->len; i++) h = (h ^ t->b[i]) * 16777619u;
    return h;
}

static int intern(const Tok *t) {
    if (g_ntoks * 2 >= g_tokHashSize) {
        g_tokHashSize = g_tokHashSize ? g_tokHashSize * 2 : 4096;
        g_tokHash = xrealloc(g_tokHash, sizeof(int) * g_tokHashSize);
        for (int i = 0; i < g_tokHashSize; i++) g_tokHash[i] = -1;
        for (int i = 0; i < g_ntoks; i++) {
            uint32_t h = tokHashOf(&g_toks[i]) & (g_tokHashSize - 1);
            while (g_tokHash[h] >= 0) h = (h + 1) & (g_tokHashSize - 1);
            g_tokHash[h] = i;
        }
    }
    uint32_t h = tokHashOf(t) & (g_tokHashSize - 1);
    for (; g_tokHash[h] >= 0; h = (h + 1) & (g_tokHashSize - 1)) {
        const Tok *u = &g_toks[g_tokHash[h]];
        if (u->w == t->w && u->len == t->len && !memcmp(u->b, t->b, t->len)) return g_tokHash[h];
    }
    if (g_ntoks == g_tokCap) {
        g_tokCap = g_tokCap * 2 + 1024;
        g_toks = xrealloc(g_toks, sizeof(Tok) * g_tokCap);
    }
    g_toks[g_ntoks] = *t;
    g_toks[g_ntoks].dict = -1;
    g_tokHash[h] = g_ntoks;
    return g_ntoks++;
}

/* SET (or LOAD for wide channels) with values vals[] for mask bits,
 * then wait (1-255). mask == 0 is a pure WAIT. */
static int makeTok(uint8_t w, uint32_t mask, const uint8_t *vals, int wait) {
    Tok t;
    memset(&t, 0, sizeof(t));
    t.w = w;
    if (w > VGR3_MASK_MAX_W && mask) {
        t.b[t.len++] = VGR3_OP_EXT;
        t.b[t.len++] = VGR3_EXT_LOAD;
        for (int i = 0; i < w; i++) t.b[t.len++] = vals[i];
        return intern(&t);   /* caller pushes the wait separately */
    }
    int k = w <= VGR3_MASK_MAX_W ? 7 - w : 7;
    uint8_t op = (uint8_t)(VGR3_OP_SET | (mask << k));
    int inl = wait < (1 << k);
    t.b[t.len++] = (uint8_t)(op | (inl ? wait : 0));
    for (int i = 0, j = 0; i < w; i++)
        if (mask >> i & 1) t.b[t.len++] = vals[j++];
    if (!inl) t.b[t.len++] = (uint8_t)wait;
    return intern(&t);
}

/* ---------------------------------------------------------------- */
/* a job: a set of channels, their tokens, and one parse of them      */

typedef struct {
    int nch;
    ChanDef def[VGR3_MAX_CHANS];
    int chStart[VGR3_MAX_CHANS], chEnd[VGR3_MAX_CHANS], chLoop[VGR3_MAX_CHANS];
    uint32_t chOut[VGR3_MAX_CHANS];   /* channel start in the blob */
    int n, cap;
    int *tok, *chanOf;
    /* parse results, per token position */
    uint8_t *bnd, *dep;
    uint32_t *opos;
    int *ctgt, *clen;
    uint16_t *ccnt;
    uint32_t size;                     /* blob size incl. dict entries */
    uint32_t forcedLoop[VGR3_MAX_REGS / 32 + 1];
} Job;

static void jobPush(Job *jb, int t) {
    if (jb->n == jb->cap) {
        jb->cap = jb->cap * 2 + 4096;
        jb->tok = xrealloc(jb->tok, sizeof(int) * jb->cap);
        jb->chanOf = xrealloc(jb->chanOf, sizeof(int) * jb->cap);
    }
    jb->chanOf[jb->n] = jb->nch;
    jb->tok[jb->n++] = t;
}

static void pushWait(Job *jb, uint8_t w, int wait) {
    while (wait > 0) {
        int c = wait > 255 ? 255 : wait;
        jobPush(jb, makeTok(w, 0, NULL, c));
        wait -= c;
    }
}

/* Builds a channel's tokens. Returns 0 (and adds nothing) if the
 * channel never writes anything. */
static int addChannel(Job *jb, ChanDef d) {
    uint32_t F = g_nframes, L = g_loopFrame;
    uint32_t *mask = xrealloc(NULL, sizeof(uint32_t) * F);
    int any = 0;
    for (uint32_t f = 0; f < F; f++) {
        mask[f] = 0;
        for (int b = 0; b < d.w; b++) {
            int a = d.base + b;
            size_t i = (size_t)f * g_nregs + a;
            if (!(d.owned >> b & 1) || !(g_wr[i] & 1)) continue;
            if ((g_wr[i] & 2) || stateAt((int32_t)f - 1, a) != g_val[i]) mask[f] |= 1u << b;
        }
        if (mask[f]) any = 1;
    }
    if (!any) { free(mask); return 0; }

    /* At the loop point, rewrite every known byte so the state after
     * the JUMP matches the first pass. Side-effect bytes are only
     * rewritten when the end state leaves them wrong. */
    if (L != VGR3_LOOP_NONE) {
        for (int b = 0; b < d.w; b++) {
            int a = d.base + b;
            if (!(d.owned >> b & 1) || (mask[L] >> b & 1)) continue;
            if (L == 0) {
                /* Looping back to frame 0 replays from power-on 0. Any
                 * owned byte not written at frame 0 but left nonzero at
                 * the end must be rewritten to 0, or the second pass
                 * would start with the first pass's final value. */
                if (stateAt(F - 1, a) <= 0) continue;
                mask[0] |= 1u << b;
                if (seCapable(a)) {
                    jb->forcedLoop[a / 32] |= 1u << (a % 32);
                    if (!g_quiet)
                        fprintf(stderr, "warning: reg 0x%02X reset at loop 0 (side effect)\n", a);
                }
                continue;
            }
            if (stateAt(L, a) < 0) continue;
            if (!seCapable(a)) mask[L] |= 1u << b;
            else if (stateAt(F - 1, a) != stateAt(L, a)) {
                mask[L] |= 1u << b;
                jb->forcedLoop[a / 32] |= 1u << (a % 32);
                if (!g_quiet)
                    fprintf(stderr, "warning: reg 0x%02X rewritten at loop point (side effect)\n", a);
            }
        }
    }

    jb->def[jb->nch] = d;
    jb->chStart[jb->nch] = jb->n;
    jb->chLoop[jb->nch] = -1;
    uint32_t f = 0;
    while (f < F && !mask[f] && f != L) f++;
    pushWait(jb, d.w, (int)f);
    while (f < F) {
        uint32_t g = f + 1;
        while (g < F && !mask[g] && g != L) g++;
        if (f == L) jb->chLoop[jb->nch] = jb->n;
        /* no loop: nothing after the last event matters, END holds */
        int wait = (g == F && L == VGR3_LOOP_NONE) ? 1 : (int)(g - f);
        uint8_t vals[16];
        int nv = 0;
        for (int b = 0; b < d.w; b++) {
            int16_t s = stateAt(f, d.base + b);
            if (d.w > VGR3_MASK_MAX_W) vals[nv++] = (uint8_t)(s < 0 ? 0 : s);
            else if (mask[f] >> b & 1) vals[nv++] = (uint8_t)(s < 0 ? 0 : s);
        }
        if (d.w > VGR3_MASK_MAX_W) {
            jobPush(jb, makeTok(d.w, mask[f], vals, 0));
            pushWait(jb, d.w, wait);
        } else {
            jobPush(jb, makeTok(d.w, mask[f], vals, wait > 255 ? 255 : wait));
            if (wait > 255) pushWait(jb, d.w, wait - 255);
        }
        f = g;
    }
    jb->chEnd[jb->nch] = jb->n;
    jb->nch++;
    free(mask);
    return 1;
}

static void jobFree(Job *jb) {
    free(jb->tok); free(jb->chanOf); free(jb->bnd); free(jb->dep);
    free(jb->opos); free(jb->ctgt); free(jb->clen); free(jb->ccnt);
    memset(jb, 0, sizeof(*jb));
}

/* ---------------------------------------------------------------- */
/* dictionary                                                         */

/* A dict entry is a literal SET token, or a CALL of cnt items at
 * token position tgt (stored in the dict area as CALLM/CALLL). */
typedef struct { int tok, tgt, cnt; } DictEnt;
static DictEnt g_dict[VGR3_DICT_MAX];
static int g_ndict;
static uint32_t g_dictBytes;
static int *g_dcAt, *g_dcNext;   /* dict CALL entries by target position */

static int dictEntLen(const DictEnt *e) {
    return e->tok >= 0 ? g_toks[e->tok].len : e->cnt <= 16 ? 3 : 4;
}

static void setDict(const DictEnt *ents, int n) {
    for (int i = 0; i < g_ndict; i++)
        if (g_dict[i].tok >= 0) g_toks[g_dict[i].tok].dict = -1;
    g_ndict = n;
    g_dictBytes = 0;
    for (int i = 0; i < n; i++) {
        g_dict[i] = ents[i];
        if (ents[i].tok >= 0) g_toks[ents[i].tok].dict = (int16_t)i;
        g_dictBytes += dictEntLen(&ents[i]);
    }
}

/* Dict index of a CALL of `items` items at position j, or -1. */
static int dictCall(int j, int items) {
    for (int e = g_dcAt[j]; e >= 0; e = g_dcNext[e])
        if (g_dict[e].cnt == items) return e;
    return -1;
}

static int litCost(int t) { return g_toks[t].dict >= 0 ? 1 : g_toks[t].len; }

/* ---------------------------------------------------------------- */
/* parse                                                              */

enum { CALL_S, CALL_M, CALL_L, NCALLFORMS };

static int callForm(int items, uint32_t dist) {
    if (items <= 16) return dist <= 255 ? CALL_S : CALL_M;
    return CALL_L;
}
static const int kCallCost[NCALLFORMS] = { 2, 3, 4 };

#define HASH_BITS 18
#define MAX_CANDS 64

typedef struct { int gain, j, L, items, dep, cost; } Match;

/* match-finder state for the parse in progress */
static int *g_head, *g_next, *g_head1, *g_next1;

/* Finds the best CALL at item i of channel c (output offset out),
 * copying only from items before lim (items from lim on aren't parsed
 * yet). gain 0 / j -1 means a literal is best. If cands is given, also
 * collects the best match for each distinct end, for lookahead. */
static Match findMatch(const Job *jb, int c, int i, int lim, uint32_t out, Match *cands, int *ncands) {
    Match best = { 0, -1, 1, 0, 0, 0 };
    int ce = jb->chEnd[c], lp = jb->chLoop[c];
    if (cands) *ncands = 0;
    if (i >= ce) return best;
#define OFFER(g_, j_, L_, it_, d_, cost_) do { \
        Match m_ = { g_, j_, L_, it_, d_, cost_ }; \
        if (m_.gain > best.gain) best = m_; \
        if (cands) { \
            int k_ = 0; \
            while (k_ < *ncands && cands[k_].L != m_.L) k_++; \
            if (k_ < *ncands) { if (m_.gain > cands[k_].gain) cands[k_] = m_; } \
            else if (k_ < MAX_CANDS) { cands[k_] = m_; (*ncands)++; } \
        } \
    } while (0)
    if (i + 1 < ce) {
        uint32_t key = ((uint32_t)jb->tok[i] * 2654435761u ^ (uint32_t)jb->tok[i + 1] * 40503u) >> (32 - HASH_BITS);
        int iEnd = ce;
        if (lp > i) iEnd = lp;   /* the loop item must stay a top-level item */
        int tries = 0;
        for (int j = g_head[key]; j >= 0 && tries < g_window; j = g_next[j], tries++) {
            if (jb->tok[j] != jb->tok[i] || jb->tok[j + 1] != jb->tok[i + 1]) continue;
            int jc = jb->chanOf[j];
            if (jc != c && !g_cross) continue;
            int jEnd = jb->chEnd[jc] < lim ? jb->chEnd[jc] : lim;
            uint32_t dist = out - jb->opos[j];
            int L = 0, items = 0, dep = 0, lit = 0;
            while (j + L < jEnd && i + L < iEnd && jb->tok[j + L] == jb->tok[i + L]) {
                if (jb->bnd[j + L]) {
                    if (items == 255 || (jb->dep[j + L] + 1 > g_maxDepth)) break;
                    items++;
                    if (jb->dep[j + L] > dep) dep = jb->dep[j + L];
                }
                lit += litCost(jb->tok[i + L]);
                L++;
                if (j + L == jEnd || jb->bnd[j + L]) {
                    int form = callForm(items, dist), cost, score;
                    if (dictCall(j, items) >= 0) score = lit - (cost = 1);
                    else score = lit - (cost = kCallCost[form]) - (form != CALL_S ? g_farPenalty : 0);
                    if (score > 0) OFFER(score, j, L, items, dep + 1, cost);
                }
            }
        }
    }
    /* one-item CALL of a single long token */
    if (litCost(jb->tok[i]) > kCallCost[CALL_S] && (lp <= i || lp > i + 1)) {
        int tries = 0;
        for (int j = g_head1[jb->tok[i]]; j >= 0 && tries < g_window; j = g_next1[j], tries++) {
            int jc = jb->chanOf[j];
            if (jc != c && !g_cross) continue;
            int form = callForm(1, out - jb->opos[j]), cost, score;
            if (dictCall(j, 1) >= 0) score = litCost(jb->tok[i]) - (cost = 1);
            else score = litCost(jb->tok[i]) - (cost = kCallCost[form]) - (form != CALL_S ? g_farPenalty : 0);
            if (score > 0) OFFER(score, j, 1, 1, 1, cost);
        }
    }
#undef OFFER
    return best;
}

static void parseJob(Job *jb) {
    int n = jb->n;
    jb->bnd = xrealloc(jb->bnd, n + 1);
    jb->dep = xrealloc(jb->dep, n + 1);
    jb->opos = xrealloc(jb->opos, sizeof(uint32_t) * (n + 1));
    jb->ctgt = xrealloc(jb->ctgt, sizeof(int) * (n + 1));
    jb->clen = xrealloc(jb->clen, sizeof(int) * (n + 1));
    jb->ccnt = xrealloc(jb->ccnt, sizeof(uint16_t) * (n + 1));
    memset(jb->bnd, 0, n + 1);
    g_head = xrealloc(NULL, sizeof(int) << HASH_BITS);
    g_next = xrealloc(NULL, sizeof(int) * (n + 1));
    g_head1 = xrealloc(NULL, sizeof(int) * g_ntoks);   /* single-token items, by token */
    g_next1 = xrealloc(NULL, sizeof(int) * (n + 1));
    for (int i = 0; i < 1 << HASH_BITS; i++) g_head[i] = -1;
    for (int t = 0; t < g_ntoks; t++) g_head1[t] = -1;
    g_dcAt = xrealloc(g_dcAt, sizeof(int) * (n + 1));
    g_dcNext = xrealloc(g_dcNext, sizeof(int) * VGR3_DICT_MAX);
    for (int i = 0; i <= n; i++) g_dcAt[i] = -1;
    for (int e = g_ndict - 1; e >= 0; e--)
        if (g_dict[e].tok < 0) { g_dcNext[e] = g_dcAt[g_dict[e].tgt]; g_dcAt[g_dict[e].tgt] = e; }

    uint32_t out = g_dictBytes;
    for (int c = 0; c < jb->nch; c++) {
        int cs = jb->chStart[c], ce = jb->chEnd[c], lp = jb->chLoop[c];
        jb->chOut[c] = out;
        for (int i = cs; i < ce;) {
            jb->bnd[i] = 1;
            jb->opos[i] = out;
            Match best;
            if (g_lookahead) {
                /* one-step lookahead: pick the end that maximizes this
                 * item's gain plus the best gain at the next item */
                Match cands[MAX_CANDS + 1];
                int nc;
                findMatch(jb, c, i, i, out, cands, &nc);
                int k = 0;
                while (k < nc && cands[k].L != 1) k++;
                if (k == nc) cands[nc++] = (Match){ 0, -1, 1, 0, 0, 0 };   /* literal */
                int bestScore = -1;
                best = cands[0];
                for (k = 0; k < nc; k++) {
                    const Match *m = &cands[k];
                    uint32_t o2 = out + (m->j >= 0 ? (uint32_t)m->cost : (uint32_t)litCost(jb->tok[i]));
                    int score = m->gain + findMatch(jb, c, i + m->L, i, o2, NULL, NULL).gain;
                    if (score > bestScore || (score == bestScore && m->gain > best.gain)) {
                        bestScore = score;
                        best = *m;
                    }
                }
            } else {
                best = findMatch(jb, c, i, i, out, NULL, NULL);
            }
            if (best.j >= 0) {
                jb->ctgt[i] = best.j; jb->clen[i] = best.L; jb->ccnt[i] = (uint16_t)best.items;
                jb->dep[i] = (uint8_t)best.dep;
                out += best.cost;
            } else {
                jb->ctgt[i] = -1; jb->clen[i] = 1; jb->ccnt[i] = 0;
                jb->dep[i] = 0;
                out += litCost(jb->tok[i]);
            }
            if (i + 1 < ce) {
                uint32_t key = ((uint32_t)jb->tok[i] * 2654435761u ^ (uint32_t)jb->tok[i + 1] * 40503u) >> (32 - HASH_BITS);
                g_next[i] = g_head[key]; g_head[key] = i;
            }
            if (jb->ctgt[i] < 0) { g_next1[i] = g_head1[jb->tok[i]]; g_head1[jb->tok[i]] = i; }
            i += jb->clen[i];
        }
        out += lp >= 0 ? 3 : 1;   /* JUMP / END */
    }
    jb->size = out;
    free(g_head);
    free(g_next);
    free(g_head1);
    free(g_next1);
}

/* Picks the dictionary from the items of the last parse: literal
 * SETs, and CALLs whose (target, count) repeats. */
typedef struct { DictEnt e; int gain; } Cand;
static int byGain(const void *a, const void *b) {
    const Cand *x = a, *y = b;
    if (x->gain != y->gain) return y->gain - x->gain;
    if (x->e.tok != y->e.tok) return x->e.tok - y->e.tok;
    return x->e.tgt != y->e.tgt ? x->e.tgt - y->e.tgt : x->e.cnt - y->e.cnt;
}
typedef struct { int tgt, cnt, save; } CallUse;
static int byCall(const void *a, const void *b) {
    const CallUse *x = a, *y = b;
    return x->tgt != y->tgt ? x->tgt - y->tgt : x->cnt - y->cnt;
}

static void chooseDict(Job *jb) {
    int *cnt = xrealloc(NULL, sizeof(int) * g_ntoks);
    Cand *cand = xrealloc(NULL, sizeof(Cand) * (g_ntoks + jb->n));
    CallUse *cu = xrealloc(NULL, sizeof(CallUse) * (jb->n + 1));
    int nc = 0, ncu = 0;
    DictEnt ents[VGR3_DICT_MAX];
    memset(cnt, 0, sizeof(int) * g_ntoks);
    for (int i = 0; i < jb->n; i++) {
        if (!jb->bnd[i]) continue;
        if (jb->ctgt[i] < 0) { cnt[jb->tok[i]]++; continue; }
        /* what a 1-byte dict CALL saves over the normal CALL form here */
        int form = callForm(jb->ccnt[i], jb->opos[i] - jb->opos[jb->ctgt[i]]);
        if (g_dictCalls) cu[ncu++] = (CallUse){ jb->ctgt[i], jb->ccnt[i], kCallCost[form] - 1 };
    }
    for (int t = 0; t < g_ntoks; t++) {
        const Tok *k = &g_toks[t];
        int g = cnt[t] * (k->len - 1) - (k->len + 2);
        if (k->b[0] >= VGR3_OP_SET && g > 0) cand[nc++] = (Cand){ { t, -1, 0 }, g };
    }
    qsort(cu, ncu, sizeof(CallUse), byCall);
    for (int i = 0; i < ncu;) {
        int k = i, g = 0;
        for (; k < ncu && cu[k].tgt == cu[i].tgt && cu[k].cnt == cu[i].cnt; k++) g += cu[k].save;
        DictEnt e = { -1, cu[i].tgt, cu[i].cnt };
        g -= dictEntLen(&e) + 2;
        if (g > 0) cand[nc++] = (Cand){ e, g };
        i = k;
    }
    qsort(cand, nc, sizeof(Cand), byGain);
    if (nc > g_dictMax) nc = g_dictMax;
    for (int i = 0; i < nc; i++) ents[i] = cand[i].e;
    setDict(ents, nc);
    free(cnt);
    free(cand);
    free(cu);
}

/* ---------------------------------------------------------------- */
/* emit                                                               */

static long g_stat[8];   /* literal SET, DICT, CALLS, CALLM, CALLL, LOAD, DICT CALL */

static void emitJob(Job *jb, ByteBuf *out) {
    ByteBuf blob = { 0 };
    uint16_t dictOfs[VGR3_DICT_MAX];
    memset(g_stat, 0, sizeof(g_stat));
    for (int d = 0; d < g_ndict; d++) {
        const DictEnt *e = &g_dict[d];
        dictOfs[d] = (uint16_t)blob.len;
        if (e->tok >= 0) { bbPut(&blob, g_toks[e->tok].b, g_toks[e->tok].len); continue; }
        /* an entry the final parse doesn't use may target a non-item */
        uint16_t tgt = jb->bnd[e->tgt] ? (uint16_t)jb->opos[e->tgt] : 0;
        if (e->cnt <= 16) { bbU8(&blob, (uint8_t)(VGR3_OP_CALLM | (e->cnt - 1))); bbU16(&blob, tgt); }
        else { bbU8(&blob, VGR3_OP_CALLL); bbU16(&blob, tgt); bbU8(&blob, (uint8_t)e->cnt); }
    }
    for (int c = 0; c < jb->nch; c++) {
        if (blob.len != jb->chOut[c]) { fprintf(stderr, "internal: channel offset mismatch\n"); exit(1); }
        for (int i = jb->chStart[c]; i < jb->chEnd[c]; i += jb->clen[i]) {
            if (blob.len != jb->opos[i]) { fprintf(stderr, "internal: item offset mismatch\n"); exit(1); }
            if (jb->ctgt[i] < 0) {
                const Tok *t = &g_toks[jb->tok[i]];
                if (t->dict >= 0) { bbU8(&blob, (uint8_t)t->dict); g_stat[1]++; }
                else { bbPut(&blob, t->b, t->len); g_stat[t->b[0] == VGR3_OP_EXT ? 5 : 0]++; }
                continue;
            }
            uint32_t tgt = jb->opos[jb->ctgt[i]], dist = (uint32_t)blob.len - tgt;
            int items = jb->ccnt[i], d = dictCall(jb->ctgt[i], items);
            if (d >= 0) { bbU8(&blob, (uint8_t)d); g_stat[6]++; continue; }
            switch (callForm(items, dist)) {
            case CALL_S: bbU8(&blob, (uint8_t)(VGR3_OP_CALLS | (items - 1))); bbU8(&blob, (uint8_t)dist); g_stat[2]++; break;
            case CALL_M: bbU8(&blob, (uint8_t)(VGR3_OP_CALLM | (items - 1))); bbU16(&blob, (uint16_t)tgt); g_stat[3]++; break;
            default: bbU8(&blob, VGR3_OP_CALLL); bbU16(&blob, (uint16_t)tgt); bbU8(&blob, (uint8_t)items); g_stat[4]++; break;
            }
        }
        if (jb->chLoop[c] >= 0) { bbU8(&blob, VGR3_OP_JUMP); bbU16(&blob, (uint16_t)jb->opos[jb->chLoop[c]]); }
        else bbU8(&blob, VGR3_OP_END);
    }
    if (blob.len != jb->size) { fprintf(stderr, "internal: size mismatch\n"); exit(1); }
    if (blob.len > 0xFFFF) { fprintf(stderr, "song data is %zu bytes, over the 64K limit\n", blob.len); exit(1); }

    bbPut(out, VGR3_MAGIC, 4);
    bbU8(out, VGR3_VERSION);
    bbU8(out, (uint8_t)g_chip);
    bbU8(out, (uint8_t)g_tickRate);
    bbU8(out, (uint8_t)jb->nch);
    bbU8(out, (uint8_t)g_ndict);
    bbU8(out, 0);
    bbU16(out, (uint16_t)blob.len);
    bbU32(out, g_nframes);
    bbU32(out, g_loopFrame);
    for (int c = 0; c < jb->nch; c++) {
        bbU8(out, jb->def[c].base);
        bbU8(out, jb->def[c].w);
        bbU16(out, (uint16_t)jb->chOut[c]);
    }
    for (int d = 0; d < g_ndict; d++) bbU16(out, dictOfs[d]);
    bbPut(out, blob.data, blob.len);
    free(blob.data);
}

/* ---------------------------------------------------------------- */
/* verify: decode the file and compare every frame with the source    */

static int verify(const uint8_t *file, const Job *jb) {
    static Vgr3Player pl;
    if (jb->nch == 0) {
        /* An empty stream decodes to an empty stream, so the frame-by-frame
         * comparison below would pass no matter what the source held. Treat
         * "nothing encoded" as a verification failure rather than a vacuous
         * success. */
        fprintf(stderr, "VERIFY: no channels encoded, nothing to verify\n");
        return 0;
    }
    if (!vgr3Init(&pl, file)) { fprintf(stderr, "VERIFY: decoder rejected file\n"); return 0; }
    uint32_t F = g_nframes, L = g_loopFrame;
    uint32_t frames = L == VGR3_LOOP_NONE ? F : F + (F - L);
    long errors = 0;
    for (uint32_t f = 0; f < frames && errors < 10; f++) {
        uint32_t sf = f < F ? f : L + (f - F);
        vgr3Frame(&pl);
        for (int a = 0; a < g_nregs; a++) {
            size_t i = (size_t)sf * g_nregs + a;
            int dirty = pl.dirty[a >> 3] >> (a & 7) & 1;
            int forced = sf == L && (jb->forcedLoop[a / 32] >> (a % 32) & 1);
            int16_t want = g_state[i];
            if ((g_wr[i] & 2) && !(dirty && pl.regs[a] == g_val[i])) {
                fprintf(stderr, "VERIFY: frame %u reg 0x%02X: side-effect write of %02X missing\n", f, a, g_val[i]);
                errors++;
            } else if (dirty && seCapable(a) && !(g_wr[i] & 1) && !forced) {
                fprintf(stderr, "VERIFY: frame %u reg 0x%02X: extra write to side-effect reg\n", f, a);
                errors++;
            } else if (want >= 0 && pl.regs[a] != want) {
                fprintf(stderr, "VERIFY: frame %u reg 0x%02X: got %02X want %02X\n", f, a, pl.regs[a], want);
                errors++;
            }
        }
        memset(pl.dirty, 0, sizeof(pl.dirty));
    }
    return errors == 0;
}

/* ---------------------------------------------------------------- */

static Layout chooseLayout(const VoiceDef *v, int vi) {
    /* fixed parse settings, so the choice doesn't depend on the search */
    int sw = g_window, sc = g_cross, sp = g_farPenalty;
    g_window = 64; g_cross = 1; g_farPenalty = 2;
    int best = 0;
    uint32_t bestSize = 0;
    for (int o = 0; o < v->nopt; o++) {
        Job jb;
        memset(&jb, 0, sizeof(jb));
        for (int c = 0; c < v->opt[o].nch; c++) addChannel(&jb, v->opt[o].ch[c]);
        parseJob(&jb);
        uint32_t size = jb.size + VGR3_CHAN_SIZE * jb.nch;
        if (g_verbose) printf("  voice %d %s option %d: %u bytes\n", vi, v->name, o, size);
        if (o == 0 || size < bestSize) { best = o; bestSize = size; }
        jobFree(&jb);
    }
    g_window = sw; g_cross = sc; g_farPenalty = sp;
    return v->opt[best];
}

/* mode 0: choose each voice's layout by encoding it alone; mode n:
 * option n-1 for every voice (or its last option). */
static void buildJob(Job *jb, int mode) {
    Layout lays[8];
    g_quiet = 1;
    for (int v = 0; v < g_nvoices; v++) {
        const VoiceDef *vd = &g_voices[v];
        lays[v] = mode == 0 ? chooseLayout(vd, v) : vd->opt[mode - 1 < vd->nopt ? mode - 1 : vd->nopt - 1];
    }
    g_quiet = mode != 0;
    jobFree(jb);
    for (int v = 0; v < g_nvoices; v++)
        for (int c = 0; c < lays[v].nch; c++) {
            if (jb->nch == VGR3_MAX_CHANS) { fprintf(stderr, "too many channels\n"); exit(1); }
            addChannel(jb, lays[v].ch[c]);
        }
    g_quiet = 0;
}

int main(int argc, char **argv) {
    const char *inPath = NULL, *outPath = NULL;
    int force = 0, loop = 0;
    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--rate") && a + 1 < argc) g_tickRate = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--depth") && a + 1 < argc) g_maxDepth = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--dict") && a + 1 < argc) g_dictMax = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--window") && a + 1 < argc) { g_window = atoi(argv[++a]); g_windowSet = 1; }
        else if (!strcmp(argv[a], "--layout") && a + 1 < argc) g_layout = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--no-cross")) { g_cross = 0; g_crossSet = 1; }
        else if (!strcmp(argv[a], "--far-penalty") && a + 1 < argc) { g_farPenalty = atoi(argv[++a]); g_farPenaltySet = 1; }
        else if (!strcmp(argv[a], "--greedy")) g_lookahead = 0;
        else if (!strcmp(argv[a], "--no-dict-calls")) g_dictCalls = 0;
        else if (!strcmp(argv[a], "--force")) force = 1;
        else if (!strcmp(argv[a], "--loop")) loop = 1;
        else if (!strcmp(argv[a], "--samples") && a + 1 < argc) g_samplesPath = argv[++a];
        else if (!strcmp(argv[a], "-v")) g_verbose = 1;
        else if (!inPath) inPath = argv[a];
        else if (!outPath) outPath = argv[a];
    }
    if (!inPath || !outPath) {
        fprintf(stderr,
            "usage: vgm2vgr3 [--rate N] [--depth N] [--dict N] [--window N] [--layout N]\n"
            "                [--no-cross] [--far-penalty N] [--force] [--loop] [-v]\n"
            "                [--samples out.dpcm] in.vgm out.vgr\n");
        return 1;
    }
    if (g_maxDepth < 0 || g_maxDepth > VGR3_MAX_DEPTH) g_maxDepth = VGR3_MAX_DEPTH;
    if (g_dictMax < 0 || g_dictMax > VGR3_DICT_MAX) g_dictMax = VGR3_DICT_MAX;

    FILE *f = fopen(inPath, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", inPath); return 1; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *file = xrealloc(NULL, (size_t)fsize + 16);
    memset(file + fsize, 0x66, 16);
    if (fread(file, 1, (size_t)fsize, f) != (size_t)fsize) { fprintf(stderr, "short read\n"); return 1; }
    fclose(f);
    if (!parseVgm(file, (size_t)fsize)) return 1;
    free(file);
    /* --loop: sources with no loop point get a top-level JUMP back to
     * frame 0 (see addChannel / vgr3_play.c's VGR3_OP_JUMP), so the file
     * repeats instead of holding the final state forever. */
    if (loop && g_loopFrame == VGR3_LOOP_NONE) g_loopFrame = 0;
    printf("chip %d, %u frames", g_chip, g_nframes);
    if (g_loopFrame != VGR3_LOOP_NONE) printf(", loop at %u", g_loopFrame);
    printf("\n");

    /* The greedy parse is sensitive to how hard it looks for matches,
     * so search a small grid of parse settings (each ~ms) and keep the
     * smallest. An option given on the command line pins its axis. */
    static const int kWindows[] = { 16, 64, 256, 1024 };
    static const int kPenalties[] = { 0, 2, 4, 6 };
    int nLay = g_layout >= 0 ? 1 : 4, nWin = g_windowSet ? 1 : 4, nCross = g_crossSet ? 1 : 2,
        nPen = g_farPenaltySet ? 1 : 4;
    DictEnt bestDict[VGR3_DICT_MAX];
    int bestN = 0;
    int bestLay = 0, bestWin = g_window, bestCross = g_cross, bestPen = g_farPenalty;
    uint32_t bestSize = 0xFFFFFFFFu;
    Job jb;
    memset(&jb, 0, sizeof(jb));
    for (int li = 0; li < nLay; li++) {
        /* layout modes: 0 = per voice (trial-encoded alone), 1+ = option li-1 everywhere */
        int mode = g_layout >= 0 ? g_layout + 1 : li;
        setDict(NULL, 0);
        buildJob(&jb, mode);
        for (int wi = 0; wi < nWin; wi++)
            for (int ci = 0; ci < nCross; ci++)
                for (int pi = 0; pi < nPen; pi++) {
                    if (!g_windowSet) g_window = kWindows[wi];
                    if (!g_crossSet) g_cross = ci;
                    if (!g_farPenaltySet) g_farPenalty = kPenalties[pi];
                    setDict(NULL, 0);
                    for (int it = 0; it < 4; it++) {
                        parseJob(&jb);
                        uint32_t size = jb.size + 2 * g_ndict + VGR3_CHAN_SIZE * jb.nch;
                        if (size < bestSize) {
                            bestSize = size; bestN = g_ndict;
                            memcpy(bestDict, g_dict, sizeof(DictEnt) * g_ndict);
                            bestLay = mode; bestWin = g_window; bestCross = g_cross; bestPen = g_farPenalty;
                        }
                        if (g_dictMax == 0) break;
                        chooseDict(&jb);
                    }
                    if (g_verbose)
                        printf("  layout %d window %4d cross %d penalty %d: best so far %u\n",
                               mode, g_window, g_cross, g_farPenalty, bestSize);
                }
    }
    g_window = bestWin; g_cross = bestCross; g_farPenalty = bestPen;
    setDict(NULL, 0);   /* the per-voice trial encodes must not see a dictionary */
    buildJob(&jb, bestLay);
    setDict(bestDict, bestN);
    parseJob(&jb);
    if (jb.size + 2 * g_ndict + VGR3_CHAN_SIZE * jb.nch != bestSize) {
        fprintf(stderr, "internal: final parse %u != best %u\n", jb.size, bestSize);
        exit(1);
    }
    printf("layout %d, window %d, cross %d, far penalty %d\n", bestLay, g_window, g_cross, g_farPenalty);

    ByteBuf out = { 0 };
    emitJob(&jb, &out);

    printf("%d channels:", jb.nch);
    for (int c = 0; c < jb.nch; c++) {
        uint32_t end = c + 1 < jb.nch ? jb.chOut[c + 1] : jb.size;
        printf(" %02X/%d=%u", jb.def[c].base, jb.def[c].w, end - jb.chOut[c]);
    }
    printf("\n");
    printf("items: SET %ld  DICT %ld  DICT CALL %ld  CALLS %ld  CALLM %ld  CALLL %ld  LOAD %ld\n",
           g_stat[0], g_stat[1], g_stat[6], g_stat[2], g_stat[3], g_stat[4], g_stat[5]);
    printf("dict %d entries (%u bytes), %d tokens distinct\n", g_ndict, g_dictBytes, g_ntoks);

    int ok = verify(out.data, &jb);
    if (!ok && !force) {
        fprintf(stderr, "verification failed, refusing to write %s (use --force)\n", outPath);
        return 1;
    }
    FILE *o = fopen(outPath, "wb");
    if (!o) { fprintf(stderr, "cannot write %s\n", outPath); return 1; }
    fwrite(out.data, 1, out.len, o);
    fclose(o);
    printf("wrote %s (%zu bytes)%s\n", outPath, out.len, ok ? ", verified" : ", VERIFY FAILED");
    if (g_samplesPath) {
        if (g_sampleBlocks == 0) {
            fprintf(stderr, "warning: no NES DPCM data blocks (type 0x%02X) in %s\n",
                    VGM_DB_NES_DPCM, inPath);
        } else {
            FILE *s = fopen(g_samplesPath, "wb");
            if (!s) { fprintf(stderr, "cannot write %s\n", g_samplesPath); return 1; }
            fwrite(g_sampleBank.data, 1, g_sampleBank.len, s);
            fclose(s);
            printf("wrote %s (%zu bytes, %d block%s, load at $C000)\n",
                   g_samplesPath, g_sampleBank.len, g_sampleBlocks,
                   g_sampleBlocks == 1 ? "" : "s");
        }
    }
    return ok ? 0 : 1;
}

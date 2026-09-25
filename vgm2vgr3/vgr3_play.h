/* vgr3_play.h -- VGR3 decoder (see vgr3_format.h).
 *
 * Call vgr3Frame() once per tick. It updates regs[] and sets a bit in
 * dirty[] for every byte written this frame; platform glue then writes
 * the dirty bytes to the hardware in whatever order the chip wants
 * (ascending works for NES/GB; SN76489 needs latch/data pairs) and
 * clears dirty[].
 *
 * RAM: VGR3_MAX_REGS + VGR3_MAX_REGS/8 bytes, plus per channel
 * 5 + 3*VGR3_MAX_DEPTH bytes on a target with 2-byte pointers.
 */

#ifndef VGR3_PLAY_H
#define VGR3_PLAY_H

#include <stdint.h>
#include "vgr3_format.h"

typedef struct {
    const uint8_t *ret;
    uint8_t left;       /* items still to run at this level */
} Vgr3Frame;

typedef struct {
    const uint8_t *pc;
    uint8_t base;
    uint8_t width;
    uint8_t k;          /* wait bits in a SET opcode */
    uint8_t wait;       /* frames until the next item */
    uint8_t sp;
    Vgr3Frame stack[VGR3_MAX_DEPTH];
} Vgr3Chan;

typedef struct {
    const uint8_t *data;    /* data blob */
    const uint8_t *dict;    /* dict offset table */
    uint8_t numChans;
    uint8_t regs[VGR3_MAX_REGS];
    uint8_t dirty[VGR3_MAX_REGS / 8];
    Vgr3Chan chans[VGR3_MAX_CHANS];
} Vgr3Player;

/* file must stay valid while playing (it is read in place). Returns 0
 * if it isn't a VGR3 file this decoder can play. */
int vgr3Init(Vgr3Player *p, const uint8_t *file);
void vgr3Frame(Vgr3Player *p);

#endif /* VGR3_PLAY_H */

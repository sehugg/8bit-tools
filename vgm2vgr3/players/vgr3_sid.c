/* vgr3_sid.c -- SID playback of a .vgr3 file, the Commodore 64 sibling
 * of vgr3_pokey.c / vgr3_nes.c.
 *
 * SID is a memory-mapped register file ($D400-$D41C), so the flush can
 * walk the dirty bitmap a byte at a time like the NES/POKEY players. The
 * catch is the order: a SID voice needs its ADSR (0x05/0x06) and
 * frequency/PW in place *before* the control byte (0x04) that carries the
 * gate. Luckily each voice's control byte shares its dirty byte with that
 * voice's AD/SR (V1 0x04 with 0x05/0x06; V2 0x0B with 0x0C/0x0D; V3 0x12
 * with 0x13/0x14), so a block flush just holds the control bit back and
 * writes it last: the other 7 bits go out ascending, then the gate.
 *
 * Registers 0x19-0x1C (POTX/OSC3/ENV3) are read-only and are never in
 * the shadow set. The encoder replays the control byte (0x04/0x0B/0x12)
 * whenever the source wrote it even if the value is unchanged, since a
 * gate write can retrigger the envelope.
 */

#include "vgr3_format.h"
#include "vgr3_play.h"
//#link "vgr3_play.c"

#include <cbm.h>   /* waitvsync() */

/* $D400-$D418, the three voices plus the filter/volume block. */
static void sidOut(uint8_t reg, uint8_t val) {
  ((unsigned char *)0xD400)[reg] = val;
}

static Vgr3Player g_player;

/* Flush the 8 registers of dirty byte r0. Everything but `holdreg` goes
 * out in ascending order, then `holdreg` (a voice's control/gate byte)
 * comes last so AD/SR land first. Pass 0x18 (the only byte-3 register)
 * when there is nothing to defer. */
static void sidFlush8(uint8_t r0, uint8_t holdreg) {
  uint8_t dirty = g_player.dirty[r0];
  uint8_t r = (uint8_t)(r0 << 3);
  uint8_t hold = (uint8_t)(1 << (holdreg & 7));
  uint8_t i;
  if (!dirty) return;
  g_player.dirty[r0] = 0;
  for (i = 0; i < 8; i++) {
    if ((dirty & (1 << i)) && !(hold & (1 << i)))
      sidOut((uint8_t)(r + i), g_player.regs[r + i]);
  }
  if (dirty & hold) sidOut(holdreg, g_player.regs[holdreg]);
}

void sidFlushVgr(void) {
  sidFlush8(0, 0x04);   /* V1: AD/SR, then gate */
  sidFlush8(1, 0x0B);   /* V2 */
  sidFlush8(2, 0x12);   /* V3 */
  sidFlush8(3, 0x18);   /* filter/volume */
}

const unsigned char MUSIC1[] = {
  #embed "ringout-sid.vgr3"
};

void main(void) {
  vgr3Init(&g_player, MUSIC1);
  while (1) {
    /* Once per tick: at 60 Hz that's once per vertical blank. */
    waitvsync();
    vgr3Frame(&g_player);
    sidFlushVgr();
  }
}

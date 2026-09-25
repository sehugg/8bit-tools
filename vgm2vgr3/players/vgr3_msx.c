/* vgr3_msx.c -- MSX AY-3-8910 (PSG) playback of a .vgr3 file
 * (see vgr3_format.h / vgr3_play.h).
 *
 * The AY-8910 is an indexed register file: write the register number to
 * port 0xA0, then the value to port 0xA1. Unlike the SN76489 there is no
 * latch/data protocol to reconstruct and no write-order hazard (the SID
 * gate is the odd one out), so each dirty register just flushes as a
 * select/data pair in ascending order. The shadow layout is 0-13 =
 * R0-R13 (see vgr3_format.h), so no window splitting is needed either.
 */

#include "vgr3_format.h"
#include "vgr3_play.h"
//#link "vgr3_play.c"

#include <cv.h>

/* MSX PSG: 0xA0 = register select, 0xA1 = data write. (cv_sound.h
 * declares the same ports when CV_MSX is defined; kept local so this
 * file is self-contained like vgr3_coleco.c.) */
__sfr __at (0xa0) psg_select;
__sfr __at (0xa1) psg_write;

inline void msxPsgOut(uint8_t reg, uint8_t val) {
  psg_select = reg;
  psg_write = val;
}

static Vgr3Player g_player;

/* Flush every dirty shadow register, one 8-register group at a time.
 * The last group is partial (14 registers), so stop at R13. */
void msxFlushVgr(void) {
  uint8_t r0, i;
  for (r0 = 0; r0 < (VGR3_NREGS_AY8910 + 7) / 8; r0++) {
    uint8_t dirty = g_player.dirty[r0];
    if (!dirty) continue;
    g_player.dirty[r0] = 0;
    for (i = 0; i < 8; i++) {
      uint8_t r = (uint8_t)((r0 << 3) + i);
      if (r >= VGR3_NREGS_AY8910) break;
      if (dirty & (1 << i))
        msxPsgOut(r, g_player.regs[r]);
    }
  }
}

const unsigned char MUSIC1[] = {
  #embed "map.vgr3"
};

void vint_handler(void) {
  vgr3Frame(&g_player);
  msxFlushVgr();
}

void main() {
  cv_set_vint_handler(vint_handler);
  cv_set_screen_active(true);
  vgr3Init(&g_player, MUSIC1);
  while (1) {
  }
}

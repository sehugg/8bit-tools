/* vgr3_nes.c -- NES APU playback of a .vgr3 file, the NES sibling of
 * vgr3_coleco.c.
 *
 * Unlike the SN76489, the NES APU is a plain memory-mapped register
 * file: byte regs[r] in the VGR3 shadow set maps to $4000+r, so the
 * flush is just "write every dirty byte" in ascending address order.
 * Ascending is the order VGM NES writes want: channel setup ($4000+
 * and the DMC's $4010-$4013) lands before the $4015 enable, and the
 * $4017 frame counter goes last.
 *
 * The decoder only marks a byte dirty when an op writes it, and the
 * encoder replays "side-effect" bytes ($4003/$4007/$400B/$400F length
 * reloads, $4015/$4017) whenever the source writes them even if the
 * value is unchanged. So a value-equal write that still has a hardware
 * effect (a length-counter reload) is preserved by the dirty bitmap.
 *
 * NES is a 6502 with only 2 KB of RAM, like the ColecoVision, so the
 * player lives in ROM and the file is read in place with #embed.
 */

#include "vgr3_format.h"
#include "vgr3_play.h"
//#link "vgr3_play.c"

#include <nes.h>

/* $4000-$4017. $4014 (sprite DMA) and $4016 (controller strobe) are
 * never in the shadow set; the encoder filters them out. */
static void nesApuOut(uint8_t reg, uint8_t val) {
  ((unsigned char *)0x4000)[reg] = val;
}

static Vgr3Player g_player;

static void nesFlush8(uint8_t r0) {
  uint8_t dirty = g_player.dirty[r0];
  uint8_t r,i;
  if (!dirty) return;
  g_player.dirty[r0] = 0;
  r = r0 << 3;
  for (i=0; i<8; i++) {
    if (dirty & 1) {
      nesApuOut(r, g_player.regs[r]);
    }
    r++;
    dirty >>= 1;
  }
}

void nesFlushVgr(void) {
  nesFlush8(0); // 0x0
  nesFlush8(1); // 0x8
  nesFlush8(2); // 0x10 - 0x17
}

const unsigned char MUSIC1[] = {
  #embed "eiffel.vgr3"
};

void main(void) {
  vgr3Init(&g_player, MUSIC1);
  while (1) {
    waitvsync();
    vgr3Frame(&g_player);
    nesFlushVgr();
  }
}

/* vgr3_pokey.c -- POKEY playback of a .vgr3 file, the Atari sibling of
 * vgr3_coleco.c / vgr3_nes.c.
 *
 * POKEY is a memory-mapped register file: byte regs[r] in the VGR3
 * shadow set maps 1:1 to the chip's register r, so the flush is just
 * "write every dirty byte" in ascending order. The layout the encoder
 * uses is:
 *
 *   0x00-0x07  AUDF1/AUDC1 .. AUDF4/AUDC4
 *   0x08       AUDCTL
 *   0x09       STIMER   (any write resets all four timers)
 *   0x0A       SKRES    (any write resets the serial port)
 *
 * Ascending order matters: AUDCTL (0x08) has to land before STIMER/
 * SKRES (0x09/0x0A), so a frame that reconfigures the timers *and*
 * resets them resets them with the new configuration. POT/POTGO
 * (0x0B-0x0F) are read-only from the program's point of view and are
 * never in the shadow set.
 *
 * The decoder only marks a byte dirty when an op writes it, and the
 * encoder replays STIMER/SKRES whenever the source wrote them even if
 * the resulting value is unchanged (both are command registers, not
 * storage). So a value-equal write that still resets the timers is
 * preserved by the dirty bitmap.
 *
 * POKEY_BASE is the Atari 8-bit map ($D200); the 5200 uses $E800.
 * The file is read in place, so it must live in ROM.
 */

#include "vgr3_format.h"
#include "vgr3_play.h"
//#link "vgr3_play.c"

#include <atari.h>

#define POKEY_BASE 0xD200

static void pokeyOut(uint8_t reg, uint8_t val) {
  ((unsigned char *)POKEY_BASE)[reg] = val;
}

static Vgr3Player g_player;

static void pokeyFlush8(uint8_t r0) {
  uint8_t dirty = g_player.dirty[r0];
  uint8_t r,i;
  if (!dirty) return;
  g_player.dirty[r0] = 0;
  r = r0 << 3;
  for (i=0; i<8; i++) {
    if (dirty & 1) {
      pokeyOut(r, g_player.regs[r]);
    }
    r++;
    dirty >>= 1;
  }
}

void pokeyFlushVgr(void) {
  pokeyFlush8(0);
  pokeyFlush8(8);
}

const unsigned char MUSIC1[] = {
  #embed "commando02.vgr3"
};

void main(void) {
  vgr3Init(&g_player, MUSIC1);
  while (1) {
    /* Once per tick: at 60 Hz that's once per vertical blank. */
    waitvsync();
    vgr3Frame(&g_player);
    pokeyFlushVgr();
  }
}

# vgm2vgr3 / VGR3

A register-based compression format and toolchain for VGM chiptune
captures. Instead of storing the original VGM command stream (a
timestamped log of raw port writes), a `.vgr3` file splits a song into
per-voice *channels*. Each channel owns a contiguous slice of a global
shadow register file and runs its own tiny opcode stream. The decoder is
one small per-tick interpreter that knows nothing about any chip; all
chip/platform knowledge lives in a thin platform-specific glue layer on
top of it.

## Why

The raw VGM stream mixes timing, chip selection, and register semantics
together, which makes it awkward to compress well and awkward to play
back on constrained 8-bit targets (Z80, 6502, ...). Splitting by voice
first means:

- A channel is just `{base, pc, wait, call stack}`. Ops write bytes into
  the register file and set a dirty bit; the platform glue flushes dirty
  bytes at end of frame in whatever order the chip needs.
- Because the register file is global, one physical voice can be split
  across several channels at zero decoder cost (NES pulse volume/duty vs.
  period, SN76489 tone period low/high bytes, ...). Splits fall on byte
  boundaries and channels never share a byte.
- Repetition is captured with token-level `CALL` backreferences into ROM
  rather than a RAM history window, so the decoder needs no ring buffer
  and no per-song working memory.
- The encoder is self-verifying: it decodes its own output with the real
  playback routine and refuses to write a file that doesn't match the
  source frame by frame.

## File format (`vgr3_format.h`)

```
Header (20 bytes)
Channel table [numChans]        (4 bytes each: base, width, u16 start)
Dict table [dictCount]          (2 bytes each: u16 offset)
<data blob: opcode streams + dictionary entries, back to back>
```

All multi-byte fields are little-endian and all addresses are 16-bit
offsets into the data blob, so a file is capped at 64 KB.

Opcodes (one byte, operands follow):

| Byte | Op | Operands |
|---|---|---|
| `1mmm mwww` | `SET`: mask in the high `K` bits, wait in the low `K` | one value byte per set mask bit, then a `u8` wait if the wait field is 0 |
| `0x00-0x5B` | `DICT`: run the SET at `dict[op]` | -- |
| `0x5C` | `EXT`: prefix for a bulk op | sub-op byte, then its operands |
| `0x5C 0x00` | `EXT`/`LOAD`: write all `W` window bytes (GB wave RAM) | `W` bytes, no wait |
| `0x5D` | `JUMP` (top-level loop) | `u16` addr |
| `0x5E` | `END`: hold forever | -- |
| `0x5F` | `CALLL` | `u16` addr, `u8` count |
| `0x6n` | `CALLS`, count `n+1` | `u8` backward distance from the opcode |
| `0x7n` | `CALLM`, count `n+1` | `u16` addr |

`K` is `7 - W` for channels up to 7 bytes wide; a `SET` with mask 0 is a
pure WAIT. Wider channels have no mask bits and write with `LOAD`. A
`CALL` runs `count` items starting at an address and may itself contain
`CALL`s, up to `VGR3_MAX_DEPTH` (4) levels deep. Every `SET` ends with a
wait >= 1; each frame a channel executes items until one sets a wait.

See `vgr3_format.h` for the register-file layouts per chip.

## Encoder: `vgm2vgr3`

```
vgm2vgr3 [--rate N] [--depth N] [--dict N] [--window N] [--layout N]
         [--no-cross] [--far-penalty N] [--force] [--loop] [-v]
         [--samples out.dpcm] in.vgm out.vgr
```

- `--rate N` (default 60): playback tick rate in Hz. Should match the
  target's vblank rate for tick-synchronous platforms.
- `--depth N` (default 4): maximum nested `CALL` depth.
- `--dict N` (default 92): dictionary entry cap.
- `--window N` / `--no-cross` / `--far-penalty N`: greedy-parse knobs.
  Left unset, the encoder searches a small grid of these per song and
  keeps the smallest result, since the greedy parse is sensitive to them.
- `--layout N`: force one channel layout instead of choosing per voice.
- `--loop`: give a source with no loop point a synthetic loop at frame 0.
- `--force`: write the file even if the self-check fails.
- `--samples out.dpcm`: also dump the NES DPCM data blocks, to be loaded
  at `$C000`.
- `-v`: per-voice/per-setting sizes.

The encoder recognizes SN76489 (latch/data protocol), AY8910, NES APU,
Game Boy DMG (including wave RAM), POKEY, and SID. One chip per file; if
several are present it encodes the first one it knows.

VGM has no dedicated SID chip. DefleMask exports SID as YM2151 register
writes (`0xB6 aa dd`) where `aa` is a SID register `$D400+aa`; the
encoder recognizes that when the YM2151 clock is non-zero and every such
register is in `0x00-0x18`. A real YM2151 (registers `>= 0x20`) is
rejected as unsupported.

## Decoder: `vgr3_play.c` / `vgr3_play.h`

The generic playback core, meant to be dropped onto a target unmodified:

```c
int  vgr3Init(Vgr3Player *p, const uint8_t *file);
void vgr3Frame(Vgr3Player *p);
```

- One `Vgr3Player` holds the shadow `regs[]`, a `dirty[]` bitmap, and one
  `Vgr3Chan` per channel. Fixed-size, no per-song history.
- `vgr3Frame()` must be called exactly once per tick per player (e.g.
  once per vblank). It updates `regs[]` and sets a bit in `dirty[]` for
  every byte written this frame.
- Platform glue then writes the dirty bytes to the hardware in whatever
  order the chip wants, and clears `dirty[]`.
- Deliberately dependency-free (no libc calls), so it compiles unmodified
  under a cross compiler like SDCC or cc65.

## Platform glue

Hardcoded, chip-specific players on top of `vgr3_play.c`:

- `vgr3_coleco.c` -- ColecoVision, single SN76489 on the fixed OUT port;
  reconstructs the latch/data byte protocol from the shadow registers.
- `vgr3_nes.c` -- NES APU, `$4000+r` for shadow byte `r`, ascending register order.
- `vgr3_pokey.c` -- Atari POKEY, `$D200+r`; ascending order keeps
  `AUDCTL` ahead of the `STIMER`/`SKRES` command registers.
- `vgr3_gb.c` -- Game Boy DMG, `$FF10+r` for shadow bytes `0x00-0x16`
  (NR10-NR52) and `0x20-0x2F` (wave RAM); ascending, with the unused
  `0x17-0x1F` hole skipped.
- `vgr3_sid.c` -- Commodore 64 SID, `$D400+r`. Flushes a dirty byte at a
  time like the others; each voice's control/gate byte is held back and
  written last within its byte so AD/SR land before the gate.
- `vgr3_msx.c` -- MSX with AY8910.

These need the target toolchain (they `#embed` a `.vgr3` file) and are
not built by this Makefile; they are reference glue for a port.

## Building and testing

```
make                 # builds vgm2vgr3
make roundtrip       # encodes every sample under samples/; vgm2vgr3
                     # writes nothing unless the decoded file matches the
                     # source frame by frame, so a clean run is a real
                     # correctness signal across the whole sample set
make roundtrip-loop  # same with --loop (exercises the loop-around state)
```

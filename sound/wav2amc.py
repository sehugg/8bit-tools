#!/usr/bin/env python3
# converter for Apple ][ Delta Modulation Sample Playback
# ?platform=apple2&file=deltamod.dasm

import sys

if len(sys.argv) != 3:
    print("Usage: wav2amc.py <wav_file> <dac_values>", file=sys.stderr)
    print("Convert WAV to Apple ][ Delta Modulation format", file=sys.stderr)
    sys.exit(1)

dacvals = int(sys.argv[2])
step = 1.0/dacvals
volume = 1+step/2

curval = 0

bits = []

with open(sys.argv[1],'r') as f:
    lines = f.readlines()
    for l in lines[2:]:
        toks = l.split()
        target = (float(toks[1]) * volume + 1) / 2
        current = curval * step
        if target > current:
            if curval < dacvals-1:
              curval += 1
            bits.append(1)
        else:
            if curval > 0:
              curval -= 1
            bits.append(0)

x = 0
bytearr = []
for i in range(0,len(bits)):
     if bits[i]:
         x |= 1 << (i&7)
     if (i&7) == 7:
         bytearr.append(x)
         x = 0

with open('%s%d.bin' % (sys.argv[1], dacvals),'wb') as outf:
  outf.write(bytearray(bytearr))

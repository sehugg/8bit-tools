#!/usr/bin/env python3

import sys

if len(sys.argv) != 2:
    print("Usage: nes2sms.py <nes_chr_file>", file=sys.stderr)
    print("Convert NES CHR file to SMS format", file=sys.stderr)
    sys.exit(1)

chr = open(sys.argv[1],'rb').read()
out = open('outsms.bin','wb')
for i in range(0,len(chr),16):
    for y in range(0,8):
        arr = [ ord(chr[i+y]), ord(chr[i+8+y]), 0, 0 ]
        out.write(bytearray(arr))
out.close()


#!/usr/bin/env python3

import sys

if len(sys.argv) != 2:
    print("Usage: bin2arr.py <binary_file>", file=sys.stderr)
    print("Convert binary file to C array", file=sys.stderr)
    sys.exit(1)

out = sys.stdout
chr = open(sys.argv[1],'rb').read()

out.write('const unsigned char ARRAY[%d] = {\n' % len(chr))

for i in range(0,len(chr)):
    out.write('0x%02x, ' % ord(chr[i]))
    if (i & 7) == 7:
        out.write('\n')

out.write('\n};\n')

#!/usr/bin/env python3

import sys, string, math, argparse
import mido

parser = argparse.ArgumentParser()
parser.add_argument('-s', '--start', type=int, default=21, help="first MIDI note")
parser.add_argument('-n', '--num', type=int, default=64, help="number of notes")
parser.add_argument('-v', '--voices', type=int, default=3, help="number of voices")
parser.add_argument('-T', '--transpose', type=int, default=0, help="transpose by half-steps")
parser.add_argument('-t', '--tempo', type=int, default=48, help="tempo")
parser.add_argument('-o', '--one', action="store_true", help="one voice per channel")
parser.add_argument('-H', '--hex', action="store_true", help="hex output")
parser.add_argument('-A', '--asm', action="store_true", help="asm output")
parser.add_argument('-L', '--locpu', action="store_true", help="one command per frame")
parser.add_argument('-C', '--compress', action="store_true", help="compress output with 0xfe back-references")
parser.add_argument('-D', '--drums', type=int, default=-1, help="drum channel")
parser.add_argument('midifile', help="MIDI file")
parser.add_argument('midichannels', nargs='?', help="comma-separated list of MIDI channels, or -")
args = parser.parse_args()

min_note = args.start
max_note = min_note + args.num - 1
max_voices = args.voices
one_voice_per_channel = args.one
tempo = args.tempo
transpose = args.transpose
coutput = not args.hex
asmoutput = args.asm
locpu = args.locpu
drumch = args.drums

fn = args.midifile

mid = mido.MidiFile(fn)

def hex1(n):
    return '%02x'%n
def hex2(n):
    return '0x%02x'%n

def compress_song(data):
    # Emit short back-references: 0xfe <offset> <length>
    #   offset = bytes to step back from the 0xfe opcode
    #   length = number of bytes copied from there
    # Referenced runs are always literal (never another reference), so a
    # one-level decoder that just moves its read pointer back and replays
    # them works without a decompression buffer.
    out = bytearray()
    refbyte = []  # True if this output byte is part of a 0xfe instruction
    pos = 0
    n = len(data)
    while pos < n:
        opos = len(out)
        best_len = 0
        best_off = 0
        max_off = min(255, opos)
        max_len = min(255, n - pos)
        for off in range(1, max_off + 1):
            src = opos - off
            l = 0
            # l < off keeps the match from overlapping the current position
            while l < max_len and l < off:
                if refbyte[src + l] or out[src + l] != data[pos + l]:
                    break
                l += 1
            if l > best_len:
                best_len = l
                best_off = off
                if l == max_len:
                    break
        if best_len >= 4:  # 3-byte instruction must save at least a byte
            out += bytes([0xfe, best_off, best_len])
            refbyte += [True, True, True]
            pos += best_len
        else:
            out.append(data[pos])
            refbyte.append(False)
            pos += 1
    return out

def decompress_song(buf):
    res = bytearray()
    p = 0
    while p < len(buf):
        b = buf[p]
        if b == 0xfe:
            off = buf[p + 1]
            ln = buf[p + 2]
            src = p - off
            for i in range(ln):
                res.append(buf[src + i])
            p += 3
        else:
            res.append(b)
            p += 1
    return bytes(res)

g_code = 0xc1
g_subs = []

def channels_for_track(track):
    channels = set()
    for msg in track:
        if msg.type == 'note_on':
            channels.add(msg.channel)
    return list(channels)
    
def note2drum(n):
    if n in [35,36,41]:
        return 0x41 # bass drum
    if n in [37,38,39,40]:
        return 0x42 # snare
    if n in [43,45,47,48]:
        return 0x43 # tom
    if n in [46,49,51,52,53,55,57]:
        return 0x44 # crash
    if n in [60,65,57,56,76]:
        return 0x45 # hi cowbell
    if n in [61,66,68,77]:
        return 0x46 # lo cowbell
    print(n)
    return -1

if not args.midichannels:
    #print(mid)
    print((mid.length, 'seconds'))
    for i, track in enumerate(mid.tracks):
        print(('Track {}: {} ({}) {}'.format(i, track.name, len(track), channels_for_track(track))))
        #for msg in track:
        #    print(msg)
else:
    gtime = 0
    curtime = 0
    nnotes = 0
    nvoices = 0
    curchans = 0
    channels = [int(x) for x in args.midichannels.split(',')]
    print('')
    #print(("// %s %s" % (mid, channels)))
    output = []
    for msg in mid:
        gtime += msg.time * tempo
        if msg.type == 'note_on' and msg.channel in channels:
            note = msg.note + transpose
            vel = msg.velocity
            t = int(math.ceil(gtime))
            if vel > 0:
                if t > curtime:
                    nvoices = 0
                    curchans = 0
                if note >= min_note and note <= max_note and nvoices < max_voices:
                    if not (one_voice_per_channel and (curchans & (1<<msg.channel))):
                        if msg.channel == drumch:
                            n = note2drum(note)
                        else:
                            n = note - min_note
                        if n >= 0 and n <= 127:
                            while curtime < t:
                                dt = min(63, t-curtime)
                                curtime += dt
                                if nnotes > 0:
                                    output.append(dt+128)
                            output.append(n)
                            nnotes += 1
                            nvoices += 1
                            curchans |= 1<<msg.channel
                            if locpu:
                              curtime += 1
    if args.compress:
        packed = compress_song(output)
        assert decompress_song(packed) == bytes(output), 'compression bug'
        output = list(packed)
    output.append(0xff)
    if asmoutput:
        print((','.join(['$'+hex1(x) for x in output])))
    elif coutput:
        print((','.join([hex2(x) for x in output])))
    else:
        bighex = ''.join([hex1(x) for x in output])
        for i in range(0,len(bighex)+32,32):
            print(('\thex', bighex[i:i+32]))

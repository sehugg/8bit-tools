#!/usr/bin/env python3

import sys, string, math, argparse, statistics

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--length', type=int, default=64, help="length of note table")
parser.add_argument('-u', '--upper', type=int, default=49, help="upper note # to test")
parser.add_argument('-m', '--metric', choices=['sum', 'avg', 'median', 'max'],
                    default='sum', help="error metric used to rank A440 candidates")
args = parser.parse_args()

def aggregate(errors, metric):
    if not errors:
        return 0
    if metric == 'sum':
        return sum(errors)
    if metric == 'avg':
        return sum(errors) / len(errors)
    if metric == 'median':
        return statistics.median(errors)
    if metric == 'max':
        return max(errors)
    raise ValueError(metric)

test_notes = args.upper
final_notes = args.length

#basehz = 63921/2
basehz = 15700/2
basehz2 = basehz
basehz3 = basehz2
s = 8

bittable = [
0b00000000,
0b00000001,
0b00010001,
0b01001001,
0b01010101,
0b10110101,
0b11011011,
0b11101111,
]

results = []

for a440 in range(4200,4600):
    errors = []
    for note in range(4,test_notes):
        notehz = a440 / 10.0 * math.pow(2.0, (note - 49) / 12.0);
        period = round(basehz * s / notehz) / s
        tonehz = basehz / period
        if period < s or period > 32*s:
            tonehz = -10000
        period2 = round(basehz2 * s / notehz) / s
        tonehz2 = basehz2 / period
        if period2 < s or period2 > 32*s:
            tonehz2 = -10000
        period3 = round(basehz3 * s / notehz) / s
        tonehz3 = basehz3 / period
        if period3 < s or period3 > 32*s:
            tonehz3 = -10000
        errors.append(min(abs(notehz-tonehz), abs(notehz-tonehz2), abs(notehz-tonehz3)))
    results.append((aggregate(errors, args.metric), a440))

results.sort()
best_error, best_a440 = results[0]
best_a440 /= 10.0
print('//', best_a440, best_error, test_notes, args.metric)

periods = []
tones = []
bits = []

print("const int note_table[%d] = {" % final_notes)
for note in range(0,final_notes):
    notehz = best_a440 * math.pow(2.0, (note - 49) / 12.0);
    bestperiod = 255*s
    bestscore = 999999
    besthz = -1
    for hz in [basehz, basehz2, basehz3]:
        period = int(round(hz * s / notehz))
        if period >= s and period <= 256*s:
            tonehz = hz * s / period
            error = abs(notehz - tonehz)
            #print(hz,tonehz,period,error)
            if error < bestscore:
                bestscore = error
                bestperiod = period
                besthz = hz
            
    #print(note, besthz, bestperiod, notehz)
    print('%d,' % bestperiod, end='')
    periods.append(int(bestperiod / s - 1))
    bits.append(bittable[bestperiod & (s-1)])
    if besthz==basehz:
        tones.append(10+0)
    elif besthz==basehz2:
        tones.append(10+1)
    elif besthz==basehz3:
        tones.append(6)
    else:
        tones.append(0)
print("};")

print("FREQZ: .byte " + str(periods))
print("DUTYZ: .byte " + str(bits))
print("TONEZ: .byte " + str(tones))

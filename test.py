#!/usr/bin/env python3

from subprocess import Popen, PIPE
import struct

def read_int(n_bytes):
        return int.from_bytes(moov.stdout.read(n_bytes), 'little')

def read_str(n_bytes):
        return moov.stdout.read(n_bytes).decode('utf-8')

files = [ 'https://www.twitch.tv/videos/709133837',
          'https://www.twitch.tv/videos/709163445' ]
start_time = 5940
start_pos = 1

moov = Popen(['./moov'], stdin=PIPE, stdout=PIPE, bufsize=1)

for f in files:
        moov.stdin.write(bytes([6]))
        moov.stdin.write(struct.pack('<I', len(f)))
        moov.stdin.write(bytes(f, encoding='utf-8'))

moov.stdin.write(bytes([7]))
moov.stdin.write(struct.pack('<Q', start_pos))

moov.stdin.write(bytes([2]))
moov.stdin.write(struct.pack('<d', float(start_time)))

moov.stdin.flush()

while moov and moov.poll() is None:
        cmd = read_int(1)
        if cmd==5:
                strlen = read_int(4)
                msg = read_str(strlen)
                print("msg=" + msg)
                if msg == 'play':
                        print('peeped')
                        moov.stdin.write(bytes([1,0]))
                if msg == 'pause':
                        print('peeped')
                        moov.stdin.write(bytes([1,1]))
                if msg[0:4] == "seek":
                        print('sookt')
                        moov.stdin.write(bytes([2]))
                        moov.stdin.write(struct.pack('<d', float(msg[5:])))
                moov.stdin.write(bytes([3]))
                fg, bg = 0xff00ffff, 0x99000000
                moov.stdin.write(struct.pack('<I', fg))
                moov.stdin.write(struct.pack('<I', bg))
                moov.stdin.write(struct.pack('<I', len(msg)))
                moov.stdin.write(bytes(msg, encoding='utf-8'))
                moov.stdin.flush()

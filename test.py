#!/usr/bin/env python3

from subprocess import Popen, PIPE
import struct

def read_int(n_bytes):
        return int.from_bytes(moov.stdout.read(n_bytes), 'little')

def read_str(n_bytes):
        return moov.stdout.read(n_bytes).decode('utf-8')

def read_double():
        return struct.unpack('d', moov.stdout.read(8))[0]

def play():
        moov.stdin.write(bytes([1,0]))
        moov.stdin.flush()

def pause():
        moov.stdin.write(bytes([1,1]))
        moov.stdin.flush()

def seek(time):
        moov.stdin.write(bytes([2]))
        moov.stdin.write(struct.pack('<d', time))
        moov.stdin.flush()

def request_status():
        moov.stdin.write(bytes([8]))
        moov.stdin.flush()

def put_chat_message(msg, fg, bg):
        moov.stdin.write(bytes([3]))
        moov.stdin.write(struct.pack('<I', fg))
        moov.stdin.write(struct.pack('<I', bg))
        moov.stdin.write(struct.pack('<I', len(msg)))
        moov.stdin.write(bytes(msg, encoding='utf-8'))
        moov.stdin.flush()

def set_playlist_position(pos):
        moov.stdin.write(bytes([7]))
        moov.stdin.write(struct.pack('<Q', pos))
        moov.stdin.flush()

def add_file(path):
        moov.stdin.write(bytes([6]))
        moov.stdin.write(struct.pack('<I', len(path)))
        moov.stdin.write(bytes(path, encoding='utf-8'))
        moov.stdin.flush()

files = [ 'https://www.twitch.tv/videos/709133837',
          'https://www.twitch.tv/videos/709163445' ]
start_time = 5940
start_pos = 1

moov = Popen(['./moov'], stdin=PIPE, stdout=PIPE, bufsize=1)
for f in files:
        add_file(f)
set_playlist_position(start_pos)
seek(start_time)

while moov and moov.poll() is None:
        cmd = read_int(1)
        if cmd==5:
                strlen = read_int(4)
                msg = read_str(strlen)
                if msg == 'play':
                        play()
                if msg == 'pause':
                        pause()
                if msg[0:4] == "seek":
                        seek(float(msg[5:]))
                if msg == 'status':
                        request_status()
                put_chat_message(msg, 0xff00ffff, 0x99000000)
        if cmd==4:
                pl_pos = read_int(8)
                pl_count = read_int(8)
                time = read_double()
                paused = read_int(1)
                print(f'{pl_pos}/{pl_count} {time} {"paused" if paused!=0 else "playing"}')

#!/usr/bin/env python3

import subprocess
import struct
import re
import sys
import threading, queue


def read_int(n_bytes):
	return int.from_bytes(moov.stdout.read(n_bytes), 'little')


def read_str(n_bytes):
	return moov.stdout.read(n_bytes).decode('utf-8')


def read_double():
	return struct.unpack('d', moov.stdout.read(8))[0]


def play():
	moov.stdin.write(bytes([1, 0]))
	moov.stdin.flush()


def pause():
	moov.stdin.write(bytes([1, 1]))
	moov.stdin.flush()


def seek(time):
	moov.stdin.write(bytes([2]))
	moov.stdin.write(struct.pack('<d', time))
	moov.stdin.flush()


status_request_id = 0


def request_status():
	global status_request_id
	moov.stdin.write(bytes([8]))
	moov.stdin.write(struct.pack('<I', status_request_id))
	moov.stdin.flush()
	status_request_id += 1


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


def parse_time(time_str):
	prog = re.compile(r'(-?\d+)')
	nums = prog.findall(time_str)
	time = 0
	for n in nums[:3]:
		time = time * 60 + int(n)
	return time


def format_time(seconds):
	seconds = int(round(seconds))
	hours, seconds = seconds // 3600, seconds % 3600
	minutes, seconds = seconds // 60, seconds % 60

	time_str = ''
	if hours > 0:
		time_str += f'{hours}:{minutes:02}'
	else:
		time_str += f'{minutes}'
	time_str += f':{seconds:02}'

	return time_str


def mhf():
	while moov and moov.poll() is None:
		cmd = read_int(1)
		if cmd == 4:
			request_id = read_int(4)
			pl_pos = read_int(8)
			pl_count = read_int(8)
			time = read_double()
			paused = read_int(1)
			sq.put(
			    {
			        "id": request_id,
			        "pl_pos": pl_pos,
			        "pl_count": pl_count,
			        "time": time,
			        "paused": paused
			    })
		if cmd == 5:
			strlen = read_int(4)
			msg = read_str(strlen)
			mq.put(msg)


files = [
    'https://www.twitch.tv/videos/709133837',
    'https://www.twitch.tv/videos/709163445'
]
start_time = 5940
start_pos = 1

moov = subprocess.Popen(
    ['./moov'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1)
for f in files:
	add_file(f)
set_playlist_position(start_pos)
seek(start_time)

sq = queue.Queue()
mq = queue.Queue()
mt = threading.Thread(target=mhf, args=())
mt.start()


def pp():
	request_status()
	status = sq.get()
	paused = status["paused"]
	if paused:
		play()
	else:
		pause()


def get_status():
	request_status()
	return sq.get()


def format_status(status):
	s = f'{status["pl_pos"]+1}/{status["pl_count"]} '
	s += f'{"paused" if status["paused"] else "playing"} '
	s += f'{format_time(status["time"])}'
	return s


while moov and moov.poll() is None:
	if not mq.empty():
		msg = mq.get()
		put_chat_message(msg, 0xfff0cf89, 0xbb000000)
		if msg == "status":
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)
		if msg == "pp":
			pp()
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)
		if msg == "play":
			play()
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)
		if msg == "pause":
			pause()
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)
		if msg[0:4] == "seek":
			seek(parse_time(msg[5:]))
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)
		if msg[0:5] == "index":
			prog = re.compile(r'(-?\d+)')
			nums = prog.findall(msg[6:])
			set_playlist_position(int(nums[0]) - 1)
			status = format_status(get_status())
			put_chat_message(status, 0xff00ffff, 0xbb000000)

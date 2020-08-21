#!/usr/bin/env python3

import re
from moov import Moov
import time


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


def format_status(status):
	s = f'{status["pl_pos"]+1}/{status["pl_count"]} '
	s += f'{"paused" if status["paused"] else "playing"} '
	s += f'{format_time(status["time"])}'
	return s


files = [
    'https://www.twitch.tv/videos/709133837',
    'https://www.twitch.tv/videos/709163445'
]
start_time = 5940
start_pos = 1

moov = Moov()

for f in files:
	moov.append(f)
moov.index(start_pos)
moov.seek(start_time)

while moov.alive():
	messages = moov.get_user_inputs()
	for msg in messages:
		moov.put_message(msg, 0xfff0cf89, 0xbb000000)

		if msg == "status":
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

		if msg == "pp":
			status = moov.get_status()
			if status["paused"]:
				moov.play()
			else:
				moov.pause()
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

		if msg == "play":
			moov.play()
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

		if msg == "pause":
			moov.pause()
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

		if msg[0:4] == "seek":
			moov.seek(parse_time(msg[5:]))
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

		if msg[0:5] == "index":
			prog = re.compile(r'(-?\d+)')
			nums = prog.findall(msg[6:])
			moov.index(int(nums[0]) - 1)
			status = moov.get_status()
			moov.put_message(format_status(status), 0xff00ffff, 0xbb000000)

	time.sleep(0.01)

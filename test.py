#!/usr/bin/env python3

from subprocess import Popen, PIPE

moov = Popen(['./moov', '-s', '1 39 22',
              'https://www.twitch.tv/videos/413768737',
              'https://www.twitch.tv/videos/410862108',
              'https://www.twitch.tv/videos/406852587'],
             stdin=PIPE, stdout=PIPE, bufsize=1, universal_newlines=True)

msg = ''
while moov and moov.poll() is None:
	char = moov.stdout.read(1)
	if char == '\0':
		moov.stdin.write('test:' + msg + '\0')
		moov.stdin.flush()
		msg = ''
	else:
		msg += char
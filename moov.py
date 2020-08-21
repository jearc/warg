import subprocess
import struct
import threading
import queue
import time


def write(file, format, *args):
	for i, c in enumerate(format):
		if c == 'B':
			file.write(struct.pack(f'=B', args[i]))
		elif c == 's':
			file.write(
			    struct.pack(
			        f'=I{len(args[i])}s', len(args[i]),
			        bytes(args[i], encoding='utf-8')))
		elif c == 'Q':
			file.write(struct.pack('=Q', args[i]))
		elif c == 'd':
			file.write(struct.pack('=d', args[i]))
		elif c == 'I':
			file.write(struct.pack('=I', args[i]))


def read(file, format):
	data = []
	for c in format:
		if c == 'B':
			data.append(int.from_bytes(file.read(1), 'little'))
		elif c == 's':
			length = int.from_bytes(file.read(4), 'little')
			data.append(file.read(length).decode('utf-8'))
		elif c == 'Q':
			data.append(int.from_bytes(file.read(8), 'little'))
		elif c == 'd':
			data.append(struct.unpack('d', file.read(8))[0])
		elif c == 'I':
			data.append(int.from_bytes(file.read(4), 'little'))
	return data


class Moov:

	def __init__(self):
		self._status_request_counter = 0
		self._proc = subprocess.Popen(
		    ['./moov'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
		self._message_queue = queue.Queue()
		self._control_queue = queue.Queue()
		self._replies = dict()
		self._replies_lock = threading.Lock()
		self._reader_thread = threading.Thread(target=self._reader)
		self._reader_thread.start()

	def _read(self, format):
		return read(self._proc.stdout, format)

	def _write(self, format, *args):
		write(self._proc.stdin, format, *args)
		self._proc.stdin.flush()

	def _reader(self):
		while self._proc and self._proc.poll() is None:
			cmd = self._read("B")[0]
			if cmd == 10:
				data = self._read("QdB")
				self._control_queue.put({
					"pl_pos": data[0],
					"time": data[1],
					"paused": data[2]
				})
			if cmd == 4:
				data = self._read("IQQdB")
				with self._replies_lock:
					self._replies[data[0]] = {
					    "pl_pos": data[1],
					    "pl_count": data[2],
					    "time": data[3],
					    "paused": data[4]
					}
			if cmd == 5:
				msg = self._read("s")[0]
				self._message_queue.put(msg)

	def _request_status(self):
		request_id = self._status_request_counter
		self._status_request_counter += 1
		self._write('BI', 8, request_id)
		return request_id

	def _await_reply(self, request_id):
		while True:
			with self._replies_lock:
				if request_id in self._replies:
					break
			time.sleep(0.01)
		reply = None
		with self._replies_lock:
			reply = self._replies[request_id]
			del self._replies[request_id]
		return reply

	def alive(self):
		return self._proc and self._proc.poll() is None

	def put_message(self, message, fg_color, bg_color):
		self._write('BIIs', 3, fg_color, bg_color, message)

	def index(self, position):
		self._write('BQ', 7, position)

	def previous(self):
		pl_pos = self.get_status()['pl_pos']
		if pl_pos - 1 >= 0:
			self.index(pl_pos - 1)

	def next(self):
		status = self.get_status()
		pl_pos = status['pl_pos']
		pl_count = status['pl_count']
		if pl_pos + 1 < pl_count:
			self.index(pl_pos + 1)

	def append(self, path):
		self._write('Bs', 6, path)

	def set_paused(self, paused):
		self._write('BB', 1, int(paused))

	def toggle_paused(self):
		paused = self.get_status()['paused']
		self.set_paused(not paused)

	def seek(self, time):
		self._write('Bd', 2, time)

	def relative_seek(self, time_delta):
		time = self.get_status()['time']
		self.seek(time + time_delta)

	def get_status(self):
		request_id = self._request_status()
		return self._await_reply(request_id)

	def get_user_inputs(self):
		inputs = list(self._message_queue.queue)
		self._message_queue.queue.clear()
		return inputs

	def get_user_control_commands(self):
		controls = list(self._control_queue.queue)
		self._control_queue.queue.clear()
		return controls

	def close(self):
		self._write('B', 9)
		self._reader_thread.join()

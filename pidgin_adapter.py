#!/usr/bin/env python3

from pydbus import SessionBus
from gi.repository import GObject
from threading import Thread
from subprocess import Popen, PIPE
from os.path import join
from os import walk
from re import search, IGNORECASE
from html2text import HTML2Text

moov_path = "/home/james/Source/moov/moov"
video_dir = "/home/james/h/Downloads"
moov_proc = None
pending_open = False
purple_conversation = None
purple_contact = None
purple = None
video_filetypes = ["mp4", "mkv", "avi", "ogm"]

def get_video_filepaths(keywords):
    videos = []
    for root, subdirs, files in walk(video_dir):
        for file in files:
            if not file.split('.')[-1] in video_filetypes:
                continue
            match = True
            for keyword in keywords:
                if not search(keyword, file, IGNORECASE):
                    match = False
                    break
            if match:
                videos.append(join(root, file))
    return sorted(videos, key=str.lower)

def send_purple_message(conversation, message):
    purple.PurpleConvImSend(purple.PurpleConvIm(conversation), message)

def handle_moov_command(account, conversation, args, from_self):
    global purple_conversation
    global pending_open
    username = purple.PurpleAccountGetAlias(account)
    if from_self:
        purple_conversation = conversation
        open_moov([username] + args)
    else:
        pending_open = (conversation, args)
        send_purple_message(conversation, "moov: awaiting confirmation...")

def parse_command(account, conversation, message, from_self):
    global pending_open
    h = HTML2Text()
    h.ignore_links = True
    message = h.handle(message).strip()
    if message.find('MOOV') == 0:
        keywords = message.split()[1:]
        videos = get_video_filepaths(keywords)
        if len(videos) > 0:
            handle_moov_command(account, conversation, videos, from_self)
        else:
            send_purple_message(conversation, "moov: no videos found")
    if message.find('YT') == 0:
        url = message.split()[1]
        handle_moov_command(account, conversation, [url], from_self)
    if message == "RESUME":
        handle_moov_command(account, conversation, ['--resume'], from_self)
    if message == "sure" and from_self:
        handle_moov_command(account, conversation, pending_open[1], True)
        pending_open = None

def on_purple_message_receive_cb(account, sender, message, conversation, flags):
    parse_command(account, conversation, message, False)
    if conversation == purple_conversation:
        send_moov_message(purple.PurpleConversationGetTitle(conversation), message)

def on_purple_message_sent_cb(account, recipient, message):
    conversation = None
    username = purple.PurpleAccountGetAlias(account)
    for conv in purple.PurpleGetConversations():
        if recipient.split('/')[0] == purple.PurpleConversationGetName(conv).split('/')[0]:
            conversation = conv
    parse_command(account, conversation, message, True)

def kill_moov():
    global moov_proc
    if moov_proc:
        moov_proc.terminate()
    moov_proc = None
    purple_conversation = None

def moov_handler_thread_f():
    global purple_conversation
    while moov_proc and moov_proc.poll() is None:
        line = moov_proc.stdout.readline()
        if line[0:3] == "MSG":
            send_purple_message(purple_conversation, line[5:-1])
    purple_conversation = None

def open_moov(args):
    global moov_proc
    kill_moov()
    moov_proc = Popen([moov_path] + args,
                      stdin=PIPE,
                      stdout=PIPE,
                      bufsize=1,
                      universal_newlines=True)
    moov_handler_thread = Thread(target=moov_handler_thread_f, args = ())
    moov_handler_thread.start()

def send_moov_message(alias, message):
    command = "MSG " + alias + ":" + message + "\n"
    if moov_proc and moov_proc.poll() is None:
        moov_proc.stdin.write(command)

bus = SessionBus()
purple = bus.get("im.pidgin.purple.PurpleService", "/im/pidgin/purple/PurpleObject")
purple.ReceivedImMsg.connect(on_purple_message_receive_cb)
purple.SentImMsg.connect(on_purple_message_sent_cb)
GObject.MainLoop().run()

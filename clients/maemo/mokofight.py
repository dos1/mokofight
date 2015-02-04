#!/usr/bin/env python

# MokoFight 0.1 very ugly pre-alpha etc.
# by dos, GPLv3

from struct import unpack_from

import pygtk
pygtk.require('2.0')
import gtk
import os
import random
import thread
import socket
import sys
import dbus, time, sys
from math import atan2, pi

from datetime import datetime, timedelta
from functools import wraps

gtk.gdk.threads_init()

PORT = 5104
BUFFER_SIZE = 64

x = y = z = 0
ingame	  = 0
hp	  = 100
enemyhp   = 100

# TODO: split Client and AccelReader to separate files

class Client():
  
	s = None
	myID = None
	players = []
	actionsLocal = []
	actionsRemote = []
	connected = False

	def get(self, n):
		# TODO: timeout
		res = ""
		while len(res) < n:
			res += self.s.recv(1)
			#print "get(): ", res, n
		return res

	def __init__(self):
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.s.connect((sys.argv[1], PORT))
		self.s.send("MOKO")
		data = self.get(5)
		print data
		
		if data == "FIGHT":
			self.s.send("H")
			self.myID = self.get(5)
			while True:
				player = self.get(5)
				if player == "NOMOR":
					break
				self.players.append(player)
			self.connected = True

			print self.myID, self.players
		else:
			print "Couldn't connect"
			self.s.close()
			
	def _new(self):
		command = self.get(2)
		if command != "EW":
			print "new command mismatch!"
			return
		player = self.get(5)
		self.players.append(player)
		self.s.send("J" + player)
		ok = self.get(4)
		print "join reply", ok
		print self.players
		
	def _old(self):
		command = self.get(2)
		if command != "LD":
			print "old command mismatch!"
			return
		player = self.get(5)
		self.players.remove(player)
		print self.players
	
	def _gameEvent(self):
		command = self.s.recv(4)
		if command != "VENT":
			print "game event command mismatch!"
			return
		gameID = self.get(10)
		subcmd = self.get(1)
		if subcmd == "A":
			hitmiss = self.get(1)
			target = self.get(5)
			hp = self.get(3)
			print "attack", hitmiss, target, hp
		if subcmd == "E":
			winner = self.get(5)
			print "end of game, winner", winner
		if subcmd == "S":
			print "game started!"
		
		nomor = self.get(5)
		if nomor != "NOMOR":
			print "game event NOMOR command mismatch!"
	
	def _gameStart(self):
		command = self.get(3)
		if command != "AME":
			print "game command mismatch!"
			return
		print "game started!"
	
	def _interpret(self, cmd):
		handlers = {
				'N': self._new,
				'O': self._old,
				'E': self._gameEvent,
				'G': self._gameStart
			};
		if cmd in handlers:
			handlers[cmd]()
		else:
			print "unknown command " + cmd
	
	def recv(self):
		print "recv start"
		while self.connected == True:
			command = self.get(1)
			print command
			self._interpret(command)

class throttle(object):
	"""
	Decorator that prevents a function from being called more than once every
	time period.

	To create a function that cannot be called more than once a minute:

			@throttle(minutes=1)
			def my_fun():
					pass
	"""
	def __init__(self, seconds=0, minutes=0, hours=0):
		self.throttle_period = timedelta(
			seconds=seconds, minutes=minutes, hours=hours
		)
		self.time_of_last_call = datetime.min

	def __call__(self, fn):
		@wraps(fn)
		def wrapper(*args, **kwargs):
			now = datetime.now()
			time_since_last_call = now - self.time_of_last_call

			if time_since_last_call > self.throttle_period:
				self.time_of_last_call = now
				return fn(*args, **kwargs)

		return wrapper

class AccelReader:
	def __init__(self):
		bus = dbus.SystemBus()
		self.accel = bus.get_object('com.nokia.mce', '/com/nokia/mce/request', 'com.nokia.mce.request')
		orientation, stand, face, x, y, z = self.accel.get_device_orientation()
		(self.x, self.y, self.z) = (x, y, z)
		self.time = time.time()

	def get_pos(self):	
		orientation, stand, face, x, y, z = self.accel.get_device_orientation()
		new_time = time.time()
		time_modifier = (new_time - self.time)
		self.time = new_time
		time_modifier = time_modifier * 2

		if time_modifier > 1:
			time_modifier = 1
		elif time_modifier < 0.1:
			time_modifier = 0.1

		self.x = self.x + (x - self.x) * time_modifier
		self.y = self.y + (y - self.y) * time_modifier
		self.z = self.z + (z - self.z) * time_modifier
		
		return (self.x, self.y, self.z)

reader = AccelReader()

sx = [0]
sy = [0]
sz = [0]
oldx = 0
oldy = 0
oldz = 0

class MokoFight:

	def playaudio (self, path, background = 1):
		backstr=""
		if background==1:
			backstr=" &"
		os.system("aplay -q sounds/"+path+backstr)

	@throttle(seconds=0.33)
	def attackX(self, a, b, status):
		print "ATTACK ON X %d %.2f %.2f %s" % (sum(sx)/len(sx), a,b,status)

        @throttle(seconds=0.33)
        def attackY(self, a, b, status):
                print "ATTACK ON Y %d %.2f %.2f %s" % (sum(sy)/len(sy), a,b,status)

        @throttle(seconds=0.33)
        def attackZ(self, a, b, status):
                print "ATTACK ON Z %d %.2f %.2f %s" % (sum(sz)/len(sz), a,b,status)


	def accelread (self):
		global x, y, z, oldx, oldy, oldz, sx, sy, sz

		(x, y, z) = reader.get_pos()
		#print x, y, z, atan2(x, -y), atan2(y, -z)
		status = ''
		a = atan2(x,-y) / pi
		b = atan2(y,-z) / pi

		sx.append(x-oldx)
		sy.append(y-oldy)
		sz.append(z-oldz)
		if len(sx) > 10:
			sx.pop(0)
			sy.pop(0)
			sz.pop(0)

		(oldx, oldy, oldz) = (x, y, z)

		if abs(b) < 0.15:
			status = 'flat'
		elif abs(a) > 0.85 and abs(b-0.5) < 0.15:
			status = 'horizontal'
		elif abs(a+0.5) < 0.15:
			status = 'vertical' 
		elif abs(a-0.5) < 0.15:
			status = 'vulnerable'
		elif abs(a) < 0.15 and abs(b+0.5) < 0.15:
			status = 'defence'

		if sum(sx)/len(sx) > 100:
			self.attackX(a,b,status)
		if sum(sy)/len(sy) > 100:
			self.attackY(a,b,status)
		if sum(sz)/len(sz) > 100 or sum(sz)/len(sz) < -100:
			self.attackZ(a,b,status)

		sys.stdout.write("%d %d %d %.2f %.2f %s             \r" % (sum(sx)/len(sx),sum(sy)/len(sy),sum(sz)/len(sz),a, b, status))
		sys.stdout.flush()
		return True

		f = open("/dev/input/event3", "r")

		maxx = maxy = maxz = 0
		minx = miny = minz = 0
		i = 0;
		while (i<3):
			block = f.read(16)
			if block[8] == "\x02":
				if block[10] == "\x00":
					x = unpack_from( "@l", block[12:] )[0]
					maxx, minx = max( x, maxx ), min( x, minx )
				if block[10] == "\x01":
					y = unpack_from( "@l", block[12:] )[0]
					maxy, miny = max( y, maxy ), min( y, miny )
				if block[10] == "\x02":
					z = unpack_from( "@l", block[12:] )[0]
					maxz, minz = max( z, maxz ), min( z, minz )
				text = "x = %3d;  y = %3d;	z = %3d" % ( x, y, z )
			i=i+1

		f.close()

		return True

	def win(self):
		global ingame
		#self.playaudio("death.wav",0)
		ingame=0

	def lose(self):
		global ingame
		self.playaudio("death.wav",0)
		ingame=0

	def update_hp(self):
		global hp
		self.hplabel.set_label("My HP: %d" % (hp))
		self.hpprogress.set_fraction(float(hp) / 100)
		if hp == 0:
			self.lose()

	def update_enemyhp(self):
		global enemyhp
		self.ehplabel.set_label("Enemy's HP: %d" % (enemyhp))
		self.ehpprogress.set_fraction(float(enemyhp) / 100)
		if enemyhp == 0:
			self.win()

	def hit (self):
		randnr=random.randint(1, 6)
		self.playaudio("whoosh%d.wav" % (randnr))
		return True

	def defend (self):
		randnr=random.randint(1, 5)
		self.playaudio("ching%d.wav" % (randnr))
		return True

	def enemy_attack_button (self, widget, data=None):
		self.enemy_attack()
		return True

	def hit (self):
		global enemyhp
		#print "hit"
		randnr=random.randint(1, 6)
		self.playaudio("whoosh%d.wav" % (randnr))
		enemyhp = enemyhp - 5
		self.update_enemyhp()
		return True

	def miss (self):
		#print "miss"
		randnr=random.randint(1, 5)
		self.playaudio("ching%d.wav" % (randnr))	
		return True

	def game(self):
		global ingame, hp, enemyhp
		hp = 100
		enemyhp = 100
		self.vboxstatus.show()
		self.update_hp()
		self.update_enemyhp()
		self.startbutton.set_sensitive(0)
		self.separator.set_sensitive(1)
		while ingame:
			self.accelread()

		self.startbutton.set_sensitive(1)
		self.separator.set_sensitive(0)
		self.vboxstatus.hide()

	def start(self, widget, data=None):
		global ingame
		self.playaudio("start.wav",0)
		ingame=1
		thread.start_new_thread(self.game, ())

	def delete_event(self, widget, event, data=None):
		print "shutting down!"
		return False

	def destroy(self, widget, data=None):
		ingame=0
		print "nap time!"
		self.playaudio("death.wav",0)
		gtk.main_quit()

	def __init__(self):
		self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
		self.window.connect("delete_event", self.delete_event)
		self.window.connect("destroy", self.destroy)
		self.window.set_border_width(10)
		self.vbox = gtk.VBox(0, 10)
		self.label = gtk.Label("MokoFight")
		self.quitbutton = gtk.Button("Bye bye")
		self.startbutton = gtk.Button("Start game")
		self.startbutton.connect("clicked", self.start, None)
		self.quitbutton.connect_object("clicked", gtk.Widget.destroy, self.window)
#	self.separator = gtk.HSeparator()
		self.separator = gtk.Button("Harakiri")
		self.separator.connect("clicked", self.enemy_attack_button, None)	
		self.separator.set_sensitive(0)

		self.hbox=gtk.HBox(0,10)
		self.vboxhp=gtk.VBox(0,10)
		self.hplabel=gtk.Label("My HP: ?")
		self.hpprogress=gtk.ProgressBar()
		self.hpprogress.set_fraction(0)
		self.vboxhp.add(self.hplabel)
		self.vboxhp.add(self.hpprogress)
		self.vboxehp=gtk.VBox(0,10)
		self.ehplabel=gtk.Label("Enemy's HP: ?")
		self.ehpprogress=gtk.ProgressBar()
		self.ehpprogress.set_fraction(0)
		self.vboxehp.add(self.ehplabel)
		self.vboxehp.add(self.ehpprogress)
		self.hbox.add(self.vboxhp)
		self.hbox.add(self.vboxehp)

		self.vboxstatus=gtk.VBox(0,10)
		self.labeldefend=gtk.Label("")
		self.vboxstatus.add(self.hbox)
		self.vboxstatus.add(self.labeldefend)

		self.vboxbottom=gtk.VBox(0,10)
		self.vboxbottom.add(self.quitbutton)
		self.vbox.add(self.label)	
		self.vbox.add(self.startbutton)
		self.vbox.add(self.vboxstatus)
		self.vbox.add(self.separator)
		self.vbox.add(self.vboxbottom)
		self.window.add(self.vbox)
		self.window.set_title("MokoFight")
		self.window.show_all()
		self.vboxstatus.hide()

	def main(self):
		client = Client()
		thread.start_new_thread(client.recv, ())

		gtk.main()

if __name__ == "__main__":
	mokofight = MokoFight()
	mokofight.main()

# vim: ts=4

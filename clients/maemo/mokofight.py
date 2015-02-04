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
import sys
from math import atan2, pi

from mokofight import accelreader, client
from mokofight.throttle import throttle

gtk.gdk.threads_init()

PORT = 5104
BUFFER_SIZE = 64

x = y = z = 0
ingame	  = 0

reader = accelreader.AccelReader()

sx = [0]
sy = [0]
sz = [0]
oldx = 0
oldy = 0
oldz = 0

class MokoFight:
	
	client = None

	def playaudio (self, path, background = 1):
		backstr=""
		if background==1:
			backstr=" &"
		os.system("aplay -q sounds/"+path+backstr)

	@throttle(seconds=0.33)
	def attackX(self, a, b, status):
		print "ATTACK ON X %d %.2f %.2f %s" % (sum(sx)/len(sx), a,b,status)
		self.client.send("AXX")

	@throttle(seconds=0.33)
	def attackY(self, a, b, status):
		print "ATTACK ON Y %d %.2f %.2f %s" % (sum(sy)/len(sy), a,b,status)
		self.client.send("AYY")

	@throttle(seconds=0.33)
	def attackZ(self, a, b, status):
		print "ATTACK ON Z %d %.2f %.2f %s" % (sum(sz)/len(sz), a,b,status)
		self.client.send("AZZ")


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

	def win(self):
		global ingame
		#self.playaudio("death.wav",0)
		self.statuslabel.set_label("You won last game!")
		ingame=0

	def lose(self):
		global ingame
		self.playaudio("death.wav")
		self.statuslabel.set_label("You lost last game!")
		ingame=0

	def update_hp(self, hp):
		self.hplabel.set_label("My HP: %d" % (hp))
		self.hpprogress.set_fraction(float(hp) / 100)

	def update_enemyhp(self, enemyhp):
		self.ehplabel.set_label("Enemy's HP: %d" % (enemyhp))
		self.ehpprogress.set_fraction(float(enemyhp) / 100)

	def hit (self):
		randnr=random.randint(1, 6)
		self.playaudio("whoosh%d.wav" % (randnr))
		return True

	def enemy_attack_button (self, widget, data=None):
		self.enemy_attack()
		return True

	def miss (self):
		#print "miss"
		randnr=random.randint(1, 5)
		self.playaudio("ching%d.wav" % (randnr))	
		return True

	def game(self, name, player):
		global ingame, hp, enemyhp
		hp = 100
		enemyhp = 100
		self.vboxstatus.show()
		self.update_hp(100)
		self.update_enemyhp(100)
		self.startbutton.hide()
		self.statuslabel.set_label("Your name: " + name + "\nOpponent: " + player)

		while ingame:
			self.accelread()

		self.startbutton.set_sensitive(1)
		self.startbutton.show()
		self.startbutton.set_label("Start game")
		self.vboxstatus.hide()

	def start(self, name, player):
		global ingame
		self.playaudio("start.wav")
		ingame=1
		thread.start_new_thread(self.game, (name, player))

	def delete_event(self, widget, event, data=None):
		if ingame:
			self.leave()
			return True
		print "shutting down!"
		return False

	def destroy(self, widget, data=None):
		ingame=0
		print "nap time!"
		self.playaudio("death.wav")
		gtk.main_quit()
		
	def join(self):
		self.startbutton.set_sensitive(False)
		self.startbutton.set_label("Waiting for opponent...")
		self.client.setAutoJoin(True)
		self.client.joinRandomPlayer()

	def leave(self):
		self.client.leave()

	def buttonHandler(self, widget, data=None):
		if ingame:
			self.leave()
		else:
			self.join()

	def __init__(self):
		self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
		self.window.connect("delete_event", self.delete_event)
		self.window.connect("destroy", self.destroy)
		self.window.set_border_width(10)
		self.vbox = gtk.VBox(0, 10)
		self.label = gtk.Label("MokoFight")
		self.startbutton = gtk.Button("Start game")
		self.startbutton.connect("clicked", self.buttonHandler, None)

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
		self.statuslabel = gtk.Label("")
		self.vbox.add(self.label)
		self.vbox.add(self.startbutton)
		self.vbox.add(self.vboxstatus)
		self.vboxstatus.add(self.hbox)
		self.vbox.add(self.statuslabel)
		self.window.add(self.vbox)
		self.window.set_title("MokoFight")
		self.window.show_all()
		self.vboxstatus.hide()

	def eventHandler(self, eventType, args = None):
		if eventType == "start":
			self.client.setAutoJoin(False)
			self.start(args[0], args[1])
		elif eventType == "attack":
			(hit, me, hp) = args
			if me:
				self.update_hp(hp)
			else:
				self.update_enemyhp(hp)
				
			if hit:
				if not me:
					self.hit()
					
			if not hit:
				if me:
					self.miss()
		elif eventType == "end":
			winner = args[0]
			if winner:
				self.win()
			else:
				self.lose()
			#self.client.disconnect()
		elif eventType == "id":
			self.statuslabel.set_label("My name: " + args[0])
				
		print eventType, args

	def main(self):
		
		self.client = client.Client(sys.argv[1], PORT)		
		self.client.setEventHandler(self.eventHandler)
		
		self.client.setAutoJoin(False)
		
		self.client.connect()
		thread.start_new_thread(self.client.recv, ())

		gtk.main()

if __name__ == "__main__":
	mokofight = MokoFight()
	mokofight.main()

# vim: ts=4

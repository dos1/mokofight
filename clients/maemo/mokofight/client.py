import socket

class Client:
  
	s = None
	myID = None
	players = []
	actionsLocal = []
	actionsRemote = []
	connected = False
	handler = None

	def get(self, n):
		# TODO: timeout
		res = ""
		while len(res) < n:
			newres = self.s.recv(1)
			if newres == "":
				self.connected = False
				print "connection dropped!"
				return res
			res += newres
			#print "get(): ", res, n
		return res

	def __init__(self, host, port):
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.s.connect((host, port))
		self.s.sendall("MOKO")
		data = self.get(5)
		print data
		
		if data == "FIGHT":
			self.s.sendall("H")
			self.myID = self.get(5)
			while True:
				player = self.get(5)
				if player == "NOMOR":
					break
				self.players.append(player)
			self.connected = True

			if len(self.players) > 0:
				self.s.sendall("J" + self.players[-1])
				ok = self.get(4)

			print self.myID, self.players
		else:
			print "Couldn't connect"
			self.s.close()
			
	def _addPlayer(self):
		command = self.get(2)
		if command != "DD":
			print "add command mismatch!"
			return
		player = self.get(5)
		self.players.append(player)
		self.s.sendall("J" + player)
		#ok = self.get(4)
		#print "join reply", ok
		self.handler("players", (self.players))
		print self.players
		
	def _delPlayer(self):
		command = self.get(2)
		if command != "EL":
			print "del command mismatch!"
			return
		player = self.get(5)
		self.players.remove(player)
		self.handler("players", (self.players))
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
			hp = int(self.get(3))
			self.handler("attack", (hitmiss == "H", target == self.myID, hp))
			print "attack", hitmiss, target, hp
		if subcmd == "E":
			winner = self.get(5)
			self.handler("end", [winner == self.myID])
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
		self.handler("start")
		print "it's a match!"
	
	def _quit(self):
		self.connected = False
	
	def _ok(self):
		command = self.get(3)
		if command == "KAY":
			print "ok"
		else:
			print "ERROR okay mismatch"
			
	def _nook(self):
		command = self.get(3)
		if command == "OOK":
			print "ERROR NOOK"
		else:
			print "ERROR nook mismatch"
	
	def _fail(self):
		command = self.get(3)
		if command == "AIL":
			print "ERROR FAIL"
		else:
			print "ERROR fail mismatch"
	
	def _interpret(self, cmd):
		handlers = {
				'A': self._addPlayer,
				'D': self._delPlayer,
				'E': self._gameEvent,
				'G': self._gameStart,
				'O': self._ok,
				'N': self._nook,
				'F': self._fail,
				'' : self._quit
			};
		if cmd in handlers:
			handlers[cmd]()
		else:
			print "unknown command " + cmd
			
	def setEventHandler(self, handler):
		self.handler = handler
	
	def send(self, msg):
		self.s.sendall(msg)
	
	def recv(self):
		print "recv start"
		while self.connected == True:
			command = self.get(1)
			#print command
			self._interpret(command)
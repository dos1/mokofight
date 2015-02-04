import dbus, time

# TODO: make some abstraction on used device

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
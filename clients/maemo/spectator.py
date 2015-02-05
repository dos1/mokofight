#!/usr/bin/env python

import sys
from mokofight import client

PORT = 5104

def eventHandler(a, b):
	print a, b

client = client.Client(sys.argv[1], PORT)
client.setEventHandler(eventHandler)
client.connect(False, True)
client.recv()
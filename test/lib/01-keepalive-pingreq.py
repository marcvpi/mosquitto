#!/usr/bin/python

# Test whether a client sends a pingreq after the keepalive time

# The client should connect to port 1888 with keepalive=4, clean session set,
# and client id 01-keepalive-pingreq
# The client should send a PINGREQ message after the appropriate amount of time
# (4 seconds after no traffic).
#
# FIXME - this test needs improving to ensure the client handles the PINGRESP
# correctly.

import os
import subprocess
import socket
import sys
import time
from struct import *

rc = 1
keepalive = 4
connect_packet = pack('!BBH6sBBHH20s', 16, 12+2+20,6,"MQIsdp",3,2,keepalive,20,"01-keepalive-pingreq")
connack_packet = pack('!BBBB', 32, 2, 0, 0);

pingreq_packet = pack('!BB', 192, 0)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.settimeout(10)
sock.bind(('', 1888))
sock.listen(5)

client_args = sys.argv[1:]
env = dict(os.environ)
env['LD_LIBRARY_PATH'] = '../../lib:../../lib/cpp'
try:
    pp = env['PYTHONPATH']
except KeyError:
    pp = ''
env['PYTHONPATH'] = '../../lib/python:'+pp
client = subprocess.Popen(client_args, env=env)

try:
    (conn, address) = sock.accept()
    conn.settimeout(keepalive+1)
    connect_recvd = conn.recv(256)

    if connect_recvd != connect_packet:
        print("FAIL: Received incorrect connect.")
        print("Received: "+connect_recvd+" length="+str(len(connect_recvd)))
        print("Expected: "+connect_packet+" length="+str(len(connect_packet)))
    else:
        conn.send(connack_packet)
        pingreq_recvd = conn.recv(256)

        if pingreq_recvd != pingreq_packet:
            print("FAIL: Received incorrect pingreq.")
            (cmd, rl) = unpack('!BB', pingreq_recvd)
            print("Received: "+str(cmd)+", " + str(rl))
            print("Expected: 192, 0")
        else:
            rc = 0

    conn.close()
finally:
    client.terminate()
    client.wait()
    sock.close()

exit(rc)


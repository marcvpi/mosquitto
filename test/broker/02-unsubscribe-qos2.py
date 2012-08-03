#!/usr/bin/python

# Test whether a SUBSCRIBE to a topic with QoS 2 results in the correct SUBACK packet.

import subprocess
import socket
import time

import inspect, os, sys
# From http://stackoverflow.com/questions/279237/python-import-a-module-from-a-folder
cmd_subfolder = os.path.realpath(os.path.abspath(os.path.join(os.path.split(inspect.getfile( inspect.currentframe() ))[0],"..")))
if cmd_subfolder not in sys.path:
    sys.path.insert(0, cmd_subfolder)

import mosq_test

rc = 1
mid = 3
keepalive = 60
connect_packet = mosq_test.gen_connect("unsubscribe-qos2-test", keepalive=keepalive)
connack_packet = mosq_test.gen_connack(rc=0)

unsubscribe_packet = mosq_test.gen_unsubscribe(mid, "qos2/test")
unsuback_packet = mosq_test.gen_unsuback(mid)

broker = subprocess.Popen(['../../src/mosquitto', '-p', '1888'], stderr=subprocess.PIPE)

try:
    time.sleep(0.5)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("localhost", 1888))
    sock.send(connect_packet)
    connack_recvd = sock.recv(256)

    if mosq_test.packet_matches("connack", connack_recvd, connack_packet):
        sock.send(unsubscribe_packet)
        unsuback_recvd = sock.recv(256)

        if mosq_test.packet_matches("unsuback", unsuback_recvd, unsuback_packet):
            rc = 0

    sock.close()
finally:
    broker.terminate()
    broker.wait()

exit(rc)


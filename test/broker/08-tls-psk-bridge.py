#!/usr/bin/python

import subprocess
import socket
import ssl
import sys
import time

if sys.version < '2.7':
    print("WARNING: SSL not supported on Python 2.6")
    exit(0)

if ssl.OPENSSL_VERSION_NUMBER < 0x10000000:
    print("WARNING: TLS-PSK not supported on OpenSSL < 1.0")
    exit(0)


import inspect, os, sys
# From http://stackoverflow.com/questions/279237/python-import-a-module-from-a-folder
cmd_subfolder = os.path.realpath(os.path.abspath(os.path.join(os.path.split(inspect.getfile( inspect.currentframe() ))[0],"..")))
if cmd_subfolder not in sys.path:
    sys.path.insert(0, cmd_subfolder)

import mosq_test

env = dict(os.environ)
env['LD_LIBRARY_PATH'] = '../../lib:../../lib/cpp'
try:
    pp = env['PYTHONPATH']
except KeyError:
    pp = ''
env['PYTHONPATH'] = '../../lib/python:'+pp


rc = 1
keepalive = 10
connect_packet = mosq_test.gen_connect("no-psk-test-client", keepalive=keepalive)
connack_packet = mosq_test.gen_connack(rc=0)

mid = 1
subscribe_packet = mosq_test.gen_subscribe(mid, "psk/test", 0)
suback_packet = mosq_test.gen_suback(mid, 0)

publish_packet = mosq_test.gen_publish(topic="psk/test", payload="message", qos=0)

broker = subprocess.Popen(['../../src/mosquitto', '-c', '08-tls-psk-bridge.conf'], stderr=subprocess.PIPE)
bridge = subprocess.Popen(['../../src/mosquitto', '-c', '08-tls-psk-bridge.conf2'], stderr=subprocess.PIPE)

try:
    time.sleep(0.5)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect(("localhost", 1888))
    sock.send(connect_packet)

    connack_recvd = sock.recv(len(connack_packet))
    if mosq_test.packet_matches("connack", connack_recvd, connack_packet):

        sock.send(subscribe_packet)

        suback_recvd = sock.recv(len(suback_packet))
        if mosq_test.packet_matches("suback", suback_recvd, suback_packet):

            pub = subprocess.Popen(['./c/08-tls-psk-bridge.test'], env=env)
            if pub.wait():
                raise ValueError
                exit(1)

            publish_recvd = sock.recv(len(publish_packet))
            if mosq_test.packet_matches("publish", publish_recvd, publish_packet):
                rc = 0

finally:
    broker.terminate()
    broker.wait()
    if rc:
        (stdo, stde) = broker.communicate()
        print(stde)
    bridge.terminate()
    bridge.wait()
    if rc:
        (stdo, stde) = bridge.communicate()
        print(stde)

exit(rc)


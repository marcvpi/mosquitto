#!/usr/bin/python

import mosquitto

mosq = mosquitto.Mosquitto("01-will-unpwd-set")

run = -1
mosq.username_pw_set("oibvvwqw", "#'^2hg9a&nm38*us")
mosq.will_set("will-topic", "will message", 2, False)
mosq.connect("localhost", 1888)
while run == -1:
    mosq.loop()

exit(run)

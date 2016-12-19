# esp8266-volume-control

Project using MicroPython on the ESP8266 that will eventually become a
digital volume controller for my home cinema system (5.1) that can be
controlled over WiFi (and perhaps a regular IR remote too).

I use three MCP42010 digital potentiometers in rheostat mode before
the amplifier (built in to the subwoofer in my case).

Goals:
* Hook it up and test it will work as intended
* Write the application software (to make it work as an actual volume
  control, and not just three digital potentiometers I can control
  from python)
* At the end: Perhaps make an Android app for a handy remote control?

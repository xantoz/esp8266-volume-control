# esp8266-volume-control

Project using MicroPython on the ESP8266 that will eventually become a
digital volume controller for my home cinema system (5.1) that can be
controlled over WiFi (and perhaps a regular IR remote too).

I use three MCP42100 digital potentiometers before the amplifier
(built in to the subwoofer in my case).

Goals/Progress log:
* Hook it up and test it will work as intended - Pretty much done
  - Use virtual ground for audio ground to put audio inbetween 0 V and
    3.3 V as seen by MCP42100
  - Changed to MCP42100 for 100 kOhm in potentiometer mode reffed
    against audio ground (was MCP42010 in Rheostat mode)
* Make a circuit diagram for documentations sake
* Write the application software - Pretty much done/in progress
  - Now have a volume control plus a custom TCP protocol (really just
    plain ASCII, out of laziness. Perhaps replace with a binary
    protocol in the future.)

* Make client software - in progres
  - Currently writing a Qt GUI for desktop, could probably be made to
    work on phones too.
  - Need to make a cmdline tool for scripting, integration into WM's
    as keybinds etc.

# esp8266-volume-control

Project using MicroPython on the ESP8266 that will eventually become a
digital volume controller for my home cinema system (5.1) that can be
controlled over WiFi (and perhaps a regular IR remote too).

I use three MCP42010 digital potentiometers in rheostat mode before
the amplifier (built in to the subwoofer in my case).

Goals/Progress log:
* Hook it up and test it will work as intended - Partially done
  * Tried it, and it worked ok with headphones as long as I was
    powering the volume controller via battery, although I got some
    wierd buzzing when using USB as the power source (because
    it's a dirty source?)
  * The resistance is too small to make much of a difference with the
    speaker system, unless some more resistance is connected in
    series. I seem to be getting some wierdness even when powering
    from 9V battery here.
  * Possible solutions too little reistance:
    * Upgrade to MCP42100 for 100 kOhm max resistance?
    * Try to connect the pots in voltage divider mode somehow? (Wiper between audio signal and audio ground)
    * Add static resistor/trim pot to bring digitally adjustable
      resistance range inside useful area (Undesireable: adjustable
      range somewhat narrow) This might be needed, anyway, to ensure
      current is limited as to not damage the wiper circuitry (see datasheet).
  * Possible solutions sparkling:
    * Bypass capacitors
    * Series capacitors? (block DC)
    * -Consider "proper" design incorporating some amps or whatever is needed, rather than this hack.-
    * Reference audio ground against 1.6 V ! (sparkling probably due to negative voltage, which MCP42XXX can't handle)
* Make a circuit diagram for documentations sake
* Write the application software (to make it work as an actual volume
  control, and not just three digital potentiometers I can control
  from python)
* At the end: Perhaps make an Android app for a handy remote control?

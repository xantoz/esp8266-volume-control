"""Implements a dummy MCP42XXX class so the server can be tested on
unix MicroPython.

"""

# ---PINOUT---
# HSCLK = D5
# HMISO = D6 (unused)
# HMOSI = D7
CS_PIN = 15                     # GPIO15 = D8
SHDN_PIN = 4                    # GPIO4 = D2
RS_PIN = 5                      # GPIO5 = D1

class MCP42XXX(object):
    P0 = 0b01                   # Potentiometer 0
    P1 = 0b10                   # Potentiometer 1
    BOTH = 0b11                 # Both potentiometers
    MAX_VALUE = 255

    def __init__(self,
                 daisyCount=1,
                 baudrate=10000,
                 cs_pin=CS_PIN,
                 shdn_pin=SHDN_PIN,
                 rs_pin=RS_PIN):
        pass


    def set_chain(self, values, channels=None):
        pass

    def set(self, nr, value, channel=BOTH):
        pass
    
    def reset(self):
        """Toggles reset pin to reset all MCP42XXX:s on reset line"""
        pass

    def shdn_all(self):
        """Sets shutdown pin low make all MCP42XXX:s enter shutdown mode"""
        pass

    def unshdn_all(self):
        """Sets shutdown pin high to make all MCP42XXX:s enter normal mode"""
        pass

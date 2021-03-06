import machine
import utime as time

# ---MCP42XXX protocol---
#
#  CS is pulled low and then two bytes (first the command byte then
#  the data byte) are sent. Command is executed when CS is again
#  pulled high.
#
#  The data byte is a regular 8 bit unsigned integer.
#  Larger value  = PW (wiper) moves closer to PA,
#  Smaller value = PW (wiper) moves closer to PB
#
#  Structure of the command byte: 0bXXCCXXPP
#    XX are ignored
#    PP selects which potentiometer is affected.
#      PP = 01 selects potentiometer 0
#      PP = 10 selects potentiometer 1
#      PP = 11 selects both
#      PP = 00 dummy command = affects neither
#    CC selects action
#      CC = 01 write data byte to potentiometers selected by PP
#      CC = 10 put potentiometers selected by PP in shutdown mode (data byte ignored)
#      CC = 00 NOP (data byte ignored)
#      CC = 11 NOP (data byte ignored)

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
        """Be careful setting a baudrate above 10000000 (10 MHz) as this
        is above spec for MCP42XXX. The datasheet also recommends not
        using more than 5.8 MHz when daisy chaining due to propagation
        delays.

        """
        # self.spi = machine.SPI(baudrate=baudrate, polarity=0, phase=0)
        # self.spi.init()
        self.spi = machine.SPI(1)
        self.spi.init(baudrate=baudrate, polarity=0, phase=0)

        self.cs   = machine.Pin(cs_pin,   mode=machine.Pin.OUT, value=1) # active low
        self.shdn = machine.Pin(shdn_pin, mode=machine.Pin.OUT, value=1) # active low
        self.rs   = machine.Pin(rs_pin,   mode=machine.Pin.OUT, value=1) # active low

        self.daisyCount = daisyCount

    # TODO: use arrays instead of lists and/or be compatible with generators/iterators
    def set_chain(self, values, channels=None):
        """Set all units in daisy chain. First element in the lists
        values and channels corresponds to the first chip (sent last),
        second element to second etc.

        Values in the list values should be an integer between 0 and
        255, the string 'shdn' or None. Values in the list channels
        should be one of 0b01, 0b10, 0b11.

        None in values == send NOP to corresponding chip. 'shdn' in
        values sends shdn command to corresponding chip (position) and
        potentiometer (corresponding value in channels).

        """
        if channels == None:
            channels = [self.BOTH]*self.daisyCount
        if (len(values) != self.daisyCount or
            len(channels) != self.daisyCount):
            raise Exception("Wrong length for values/channels parameter")

        self.cs.low()           # enable CS
        # TODO: build bytearray with all commands ahead of time (push things out faster through SPI at cost of some extra memory)
        for value, channel in zip(reversed(values), reversed(channels)):
            chan = (channel & 0b11)
            if value == 'shdn':
                self.spi.write(bytes([0b00100000 | chan, 0x00])) # send SHDN
            elif value == None:
                self.spi.write(bytes([0b00110000, 0x00])) # send NOP
            else:
                self.spi.write(bytes([0b00010000 | chan, value & 0xff])) # normal write
        # Need to wait for HSPI peripheral to finish before disableing CS
        time.sleep_ms(2)        # TODO: make this depend on baudrate (currently set for 10000 baud empirically using logic analyzer)
        self.cs.high()          # disable CS

    def set(self, nr, value, channel=BOTH):
        """Send command to a single chip in daisy chain."""

        values = [None]*self.daisyCount
        channels = [0b00]*self.daisyCount
        try:
            values[nr] = value
            channels[nr] = channel
        except IndexError:
            raise Exception("Trying to adress non-existent chip")
        self.set_chain(values, channels)

    def reset(self):
        """Toggles reset pin to reset all MCP42XXX:s on reset line"""
        self.rs.low()
        time.sleep(1.6e-7)      # needs to be low for at least 150 ns
        self.rs.high()

    def shdn_all(self):
        """Sets shutdown pin low make all MCP42XXX:s enter shutdown mode"""
        self.shdn.low()

    def unshdn_all(self):
        """Sets shutdown pin high to make all MCP42XXX:s enter normal mode"""
        self.shdn.high()

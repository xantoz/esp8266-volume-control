import sys
import math
import usocket as socket
import uerrno as errno

if sys.platform == 'linux':
    # Use dummy MCP42XXX for testing on Linux
    MCP42XXX = __import__('mcp42xxx_dummy').MCP42XXX
else:
    MCP42XXX = __import__('mcp42xxx').MCP42XXX

# A logarithmic mapping from 0 to 99 (volume levels) to 0 to 255
# (values we send to the potentiometers). We need this because the
# MCP42010 we use is not a logarithmic potentiometer, and volume
# potentiometers usually are logarithmic due to how sound is
# perceived. Unfortunately this mapping gives us a bit less precision
# than the potentiometers themselves allow for, and some of the upper
# positions are bound to be duplicated (a quick calculation gives 89
# unique volume levels), but doing this is the next best thing to be
# able to sanely adjust volume level without getting a proper
# logarithmic digital potentiometer.
g_logarithmic_mapping = [0] + [int(MCP42XXX.MAX_VALUE*(math.log(i)/math.log(100)) + 1.0) for i in range(1, 100)]

# Our 6 channel volume controller
class VolumeController(object):
    NUMCHANNELS = 6
    NUMPOTS = NUMCHANNELS // 2  # MCP42XXX has two channels

    MAX_LEVEL = 99
    MIN_LEVEL = 0

    L = 0
    R = 1
    LR = 2
    # convenience synonyms to use when adressing the SUB/CEN channel (channel 1 == second pot)
    SUB = L
    CEN = R

    def __init__(self):
        self.pot = MCP42XXX(baudrate=40000, daisyCount=self.NUMPOTS)
        self.levels = [[0,0] for _ in range(self.NUMPOTS)] # TODO: initialize from values stored to flash (re-store periodically, or on request)
        self.master = self.MAX_LEVEL
        self.mutes  = [[False,False] for _ in range(self.NUMPOTS)]
        self.push_levels()
        self.mute_state = False

    def reset(self):
        """Resets the volume controller. Do not confuse with the RESET pin on
           MCP42XXX (which isn't used by this method).

        """
        self.levels = [[0,0] for _ in range(self.NUMPOTS)] # TODO: memset instead of realloc
        self.master = self.MAX_LEVEL
        self.push_levels()
        self.unmute()

    def volume_sweep(self):
        """Test routine that sweeps the volume of all potentiometers"""
        global g_logarithmic_mapping

        while True:
            for i in g_logarithmic_mapping:
                self.pot.set_chain([i, i, i])
            for i in reversed(g_logarithmic_mapping):
                self.pot.set_chain([i, i, i])

    def push_levels(self):
        """Sets the actual value of the pots to correspond to self.levels"""
        global g_logarithmic_mapping

        # TODO: restructure self.levels and self.mutes in a way that requires less zipping here...
        for chan, values in zip([MCP42XXX.P0, MCP42XXX.P1], zip(zip(*self.levels), zip(*self.mutes))):
            # Since we always send everything we neatly avoid the glitch where a channel is unshdn:ed by even a regular NOP
            self.pot.set_chain(['shdn' if mute else g_logarithmic_mapping[level*self.master//self.MAX_LEVEL]
                                for level, mute in zip(*values)],
                               [chan]*self.NUMPOTS)

    def set_volume(self, schannel, lr, level):
        """Set the volume of a particular channel, possibly setting both
           channels (LR) of a stereo channel at the same time.

           schannel is a number from 0 to NUMPOTS selecting what
           stereo channel to affect. lr is one of the constants L, R,
           or LR (SUB, CEN are simply convenient synonyms). level is a
           number between 0 and MAX_LEVEL or None (for muting).

        """
        if schannel < 0 or schannel > self.NUMPOTS:
            # perhaps a bit unneccesary as array accesses are always
            # bounds-checked in python, but I like a custom message
            raise ValueError("schannel out of bounds")
        if lr not in (self.L, self.R, self.LR):
            raise ValueError("lr out of bounds")
        if level != None and (level > self.MAX_LEVEL or level < self.MIN_LEVEL):
            raise ValueError("level out of bounds")

        if lr == self.LR:
            self.levels[schannel][self.L] = level
            self.levels[schannel][self.R] = level
        else:
            self.levels[schannel][lr] = level

        # TODO: only push_levels if something actually changed
        self.push_levels()

    def set_mute(self, schannel, lr, state):
        """Set mute state of a particular channel.

           schannel is a number from 0 to NUMPOTS selecting what
           stereo channel to affect. lr is one of the constants L, R,
           or LR (SUB, CEN are simply convenient synonyms). state is a
           boolean True for mute, False for not mute.
        """
        state = bool(state)
        if schannel < 0 or schannel > self.NUMPOTS:
            raise ValueError("schannel out of bounds")
        if lr not in (self.L, self.R, self.LR):
            raise ValueError("lr out of bounds")

        if lr == self.LR:
            self.mutes[schannel][self.L] = state
            self.mutes[schannel][self.R] = state
        else:
            self.mutes[schannel][lr] = state

        # TODO: only push_levels if something actually changed
        self.push_levels()


    def get_volume(self, schannel, lr):
        """Same parameters as set_volume, except it gets the volume instead.
           For lr == LR grab the max of L/R channel volumes (since
           this makes since from the usage perspective of the
           incrementer/decrementer.

        """
        if schannel < 0 or schannel > self.NUMPOTS:
            raise ValueError("schannel out of bounds")
        if lr not in (self.L, self.R, self.LR):
            raise ValueError("lr out of bounds")

        if lr == self.LR:
            return max(self.levels[schannel][self.L], self.levels[schannel][self.R])
        else:
            return self.levels[schannel][lr]

    def set_master(self, level):
        """Set the master volume level (scales down the value sent to all other pots)."""
        self.master = level
        self.push_levels()

    def get_master(self):
        """Get the master volume level"""
        return self.master

    def mute(self):
        """Set global mute state"""
        self.pot.shdn_all()     # Implemented by pulling SHDN pin low
        self.mute_state = True

    def unmute(self):
        """Unset global mute state"""
        self.pot.unshdn_all()
        self.mute_state = False
        # when bringing SHDN pin high MCP42XXX will remove SHDN status
        # from any pot that was put in this state through a command,
        # thus we need to resend the volume controller state to the
        # daisy chain so that individually muted channels won't be
        # unmuted
        self.push_levels()

    def get_status_string(self):
        """Returns a string describing the state of the volume controller.
           NOTE: This string is used directectly by VolumeServer, it thus forms part of the protocol.
           Format: <pot nr>: (<left level>, <right level>, <left mute state>, <right mute state>); ...; Master: <master level> Mute: <global mute state>
        """
        # TODO: Switch over to symbolic mapped names instead? (FL, FR, RR, RL, CEN, SUB etc.)
        # TODO: Build bytearray iteratively in loop instead (speed & memory usage improvement)?
        return "; ".join(("{}: ({},{},{},{})".format(i, schan[self.L], schan[self.R], int(smute[self.L]), int(smute[self.R]))
                          for i, (schan, smute) in enumerate(zip(self.levels, self.mutes)))) \
                    + "; Master: {} Mute: {}".format(self.master, int(self.mute_state))

    _chan_table = {
        'FL': (0, L), 'FR': (0, R), 'F': (0, LR),
        'CEN': (1, CEN), 'SUB': (1, SUB), 'CENSUB': (1, LR),
        'RL': (2, L), 'RR': (2, R), 'R': (2, LR)
    }

    @classmethod
    def get_chan(cls, chan):
        """Convert from string description of channel to (<pot ID>, <L/R>)
        tuple that can be used with set_volume etc. chan is a string
        such as FL,FR etc. (see _chan_table)
        """
        try:
            return cls._chan_table[chan]
        except KeyError:
            raise ValueError("bad channel")


import sys
import math
import usocket as socket
from mcp42xxx import MCP42XXX

# TODO: maybe move networking/protocol portion outside VolumeController class to its own class or module?

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
        self.pot = MCP42XXX(daisyCount=self.NUMPOTS)
        self.levels = [[0,0] for _ in range(self.NUMPOTS)] # TODO: initialize from value stored to flash (re-store periodically, or on request)
        self.mutes  = [[False,False] for _ in range(self.NUMPOTS)]
        self.push_levels()
        self.mute_state = False

    def reset(self):
        """Resets the volume controller. Do not confuse with the RESET pin on
           MCP42XXX (which isn't used by this method).

        """
        self.levels = [[0,0] for _ in range(self.NUMPOTS)]
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
            self.pot.set_chain(['shdn' if mute else g_logarithmic_mapping[level] for level, mute in zip(*values)],
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

    def mute(self):
        """Set global mute state"""
        self.pot.shdn_all()     # Implemented by pulling SHDN pin low
        self.mute_state = True

    def unmute(self):
        """Unset global mute state"""
        self.pot.unshdn_all()
        self.mute_state = False

    def get_status_string(self):
        """Returns a string describing the state of the volume controller.
           Format: <pot nr>: (<left level>, <right level>, <left mute state>, <right mute state>); ...; Mute: <global mute state>"""
        # TODO: Switch over to symbolic mapped names instead? (FL, FR, RR, RL, CEN, SUB etc.)
        return "; ".join(["{}: ({},{},{},{})".format(i, schan[self.L], schan[self.R], int(smute[self.L]), int(smute[self.R]))
                          for i, (schan, smute) in enumerate(zip(self.levels, self.mutes))]) \
                    + "; Mute: {}".format(int(self.mute_state))

    def server_loop(self, port, bindaddr="0.0.0.0"):
        """Start listening on port 'port' (bound to bindaddr) for volume
           control commands. The protocol is composed of plaintext
           ASCII commands delimited by newlines. Arguments are
           separated by whitespace. The first argument is the command
           to run while the following ones are its arguments.

        """
        addr = socket.getaddrinfo(bindaddr, port)[0][-1]

        s = socket.socket()
        try:
            s.bind(addr)
            s.listen(1)

            print("{}: listening on {}".format(self.__qualname__, addr))

            while True:
                cl, addr = s.accept()
                try:
                    print('{}: client connected from {}'.format(self.__qualname__, addr))
                    self._client(cl, addr)
                finally:
                    cl.close()
        finally:
            s.close()

    ## PRIVATE ##

    def _client(self, cl, addr):
        """Process for handling client."""
        # TODO: Try to allow several clients connected at once using
        #       some fancy slicing or something like that (implement
        #       threads using timers & generators or something, yay!)? coroutines?
        #       Or simply change protocol to one command per connection?

        def send_string(string):
            cl.send(bytes(string, 'ascii'))
            cl.send(b'\n')

        def send_error_msg(msg):
            print("ERROR:",msg)
            send_string("ERROR " + msg)

        while True:
            line = cl.readline().decode('ascii')
            print("{}: got cmd '{}'".format(self.__qualname__, line.rstrip())) # DEBUG (remove later)

            if not line or line == '\r\n' or line == '\n':
                break
            if line[:6] == 'byebye':
                send_string("CYA")
                break
            
            try:
                self._process_cmd(line)
            except TypeError as e:
                send_error_msg("wrong amount of args")
                sys.print_exception(e)
            except KeyError as e:
                send_error_msg("no such command")
                sys.print_exception(e)
            except ValueError as e:
                send_error_msg("bad argument: " + str(e))
                sys.print_exception(e)
            else:
                send_string("OK " + self.get_status_string())
        print("{}: client {} disconnected".format(self.__qualname__, addr)) # DEBUG (remove later)

    _chan_table = {
        'FL': (0, L), 'FR': (0, R), 'F': (0, LR),
        'CEN': (1, CEN), 'SUB': (1, SUB), 'CENSUB': (1, LR),
        'RL': (2, L), 'RR': (2, R), 'R': (2, LR)
    }

    @classmethod
    def _get_chan(cls, chan):
        """chan is a string such as FL,FR etc. (see _chan_table)"""
        try:
            return cls._chan_table[chan]
        except KeyError:
            raise ValueError("bad channel")

    def _cmd_set(self, chan, level):
        """Command to set a channel.
           Usage: set <chan> <0-99>"""
        schan, lr = self._get_chan(chan)
        self.set_volume(schan, lr, int(level))

    def _cmd_mutechan(self, chan, state):
        """Command to mute/unmute a single channel
           Usage: mute <chan> <0/1>"""
        schan, lr = self._get_chan(chan)
        self.set_mute(schan, lr, bool(int(state)))

    def _cmd_inc(self, chan):
        schan, lr = self._get_chan(chan)
        level = self.get_volume(schan, lr)
        if level < self.MAX_LEVEL:
            self.set_volume(schan, lr, level + 1)

    def _cmd_dec(self, chan):
        schan, lr = self._get_chan(chan)
        level = self.get_volume(schan, lr)
        if level > self.MIN_LEVEL:
            self.set_volume(schan, lr, level - 1)

    def _cmd_mute(self, state):
        """Command to mute/unmute all channels.
           Usage: mute <0/1>"""
        state = bool(int(state))
        if state == False:
            self.unmute()
        else:
            self.mute()

    def _cmd_status(self):
        """Basically a nop, since status is always sent to the client after
           any successful command.
        """
        pass

    # Used by _process_cmd
    _dispatch_table = {'set': _cmd_set,
                       'inc': _cmd_inc,
                       'dec': _cmd_dec,
                       'status': _cmd_status,
                       'mute': _cmd_mute,
                       'mutechan': _cmd_mutechan,
                       'reset': reset}

    def _process_cmd(self, line):
        banana = line.split()
        cmd, args = banana[0], banana[1:]
        self._dispatch_table[cmd](self, *args)

def main():
    """Main function for the Volume Controller application"""
    vc = VolumeController()
    return vc.server_loop(port=1128)

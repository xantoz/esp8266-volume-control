import sys
import usocket as socket
import uerrno as errno
from volume_control import VolumeController

class VolumeServer(object):
    """Abstract base class for the volume controller server. Implements
       the textual commands used in the protocol.

    """

    def __init__(self):
        self.vc = VolumeController()

    def _cmd_set(self, chan, level):
        """Command to set a channel.
           Usage: set <chan> <0-99>"""
        schan, lr = self.vc.get_chan(chan)
        self.vc.set_volume(schan, lr, int(level))

    def _cmd_mutechan(self, chan, state):
        """Command to mute/unmute a single channel
           Usage: mute <chan> <0/1>"""
        schan, lr = self._get_chan(chan)
        self.set_mute(schan, lr, bool(int(state)))

    def _cmd_inc(self, chan):
        schan, lr = self.vc.get_chan(chan)
        level = self.vc.get_volume(schan, lr)
        if level < self.vc.MAX_LEVEL:
            self.vc.set_volume(schan, lr, level + 1)

    def _cmd_dec(self, chan):
        schan, lr = self.vc.get_chan(chan)
        level = self.vc.get_volume(schan, lr)
        if level > self.vc.MIN_LEVEL:
            self.vc.set_volume(schan, lr, level - 1)

    def _cmd_mute(self, state):
        """Command to mute/unmute all channels.
           Usage: mute <0/1>"""
        state = bool(int(state))
        if state == False:
            self.vc.unmute()
        else:
            self.vc.mute()

    def _cmd_reset(self, state):
        """Command to reset VolumeController.
           Usage: reset"""
        self.vc.reset()

    def _cmd_status(self):
        """Basically a nop, since status is always sent to the client after
           any successful command. (TCP server case)
        """
        pass

    # Used by _process_cmd
    # TODO: make it easier for subclasses to redefine this?
    _dispatch_table = {'set': _cmd_set,
                       'inc': _cmd_inc,
                       'dec': _cmd_dec,
                       'status': _cmd_status,
                       'mute': _cmd_mute,
                       'mutechan': _cmd_mutechan,
                       'reset': _cmd_reset}

    def process_cmd(self, line):
        banana = line.split()
        cmd, args = banana[0], banana[1:]
        self._dispatch_table[cmd](self, *args)


class TCPVolumeServer(Server):
    """Implements the TCP protocol with persistent connections etc.

       Unique features:
          + Always responds with a status message to any command.
          + Send 'byebye\n' to end connection (or just close your socket)
    """

    def __init__(self):
        super().__init__()

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
            s.listen()

            print("{}: listening on {}".format(self.__qualname__, addr))

            while True:
                cl, addr = s.accept()
                try:
                    print('{}: client connected from {}'.format(self.__qualname__, addr))
                    self._client(cl, addr)
                except OSError as e:
                    print("ERROR: Got", e)
                    sys.print_exception(e)
                    # TODO: Do we need to handle errno.ECONNRESET specially?
                # TODO: seems like we might need to handle NameError? (or is my code just buggy?)
                finally:
                    cl.close()
        finally:
            s.close()

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
                self.process_cmd(line)
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


class UDPVolumeServer(Server):
    def __init__(self):
        super().__init__()

    def server_loop(self, port, bindaddr="0.0.0.0"):
        # TODO: Implement me
        pass

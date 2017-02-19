import sys
import usocket as socket
import uerrno as errno
import uselect as select
from volume_control import VolumeController

READ_ONLY = select.POLLIN | select.POLLHUP | select.POLLERR
READ_WRITE = READ_ONLY | select.POLLOUT

class VolumeServer(object):
    """Base class for the volume controller servers. Implements the
       commands used in the protocol.
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
        schan, lr = self.get_chan(chan)
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


class TCPVolumeServer(VolumeServer):
    """Implements a row-based (commands are delineated with newline)
       textual TCP protocol with persistent connections.

       The protocol is composed of plaintext ASCII commands delimited
       by newlines. Arguments are separated by whitespace. The first
       argument is the command to run while the following ones are its
       arguments.

       Unique features:
          + Always responds with a status message to any command.
          + Send 'byebye\n' to end connection (or just close your socket)

    """

    def __init__(self):
        super().__init__()

    def server_loop(self, port, bindaddr="0.0.0.0"):
        """Start listening on port 'port' (bound to bindaddr) for volume
           control commands.

        """

        # TODO: split up into init (goes into __init__) and loop body.
        # That way we can generalize the server loop (implement in
        # VolumeServer), and even do more advanced things, such as
        # running it on a timer (basically a poor mans thread).

        addr = socket.getaddrinfo(bindaddr, port)[0][-1]
        s = socket.socket()
        s.setblocking(0)

        poll = select.poll()
        poll.register(s, READ_ONLY)
        # Keep a set of all clients so we can disconnect them properly
        # in case of a fatal error. We can't use set() because sockets
        # aren't hashable.
        clientset = []

        try:
            s.bind(addr)
            s.listen(5)

            print("{}: listening on {}".format(self.__qualname__, addr))

            while True:
                for res in poll.poll():
                    print("{}: poll: '{}'".format(self.__qualname__, res)) # DEBUG
                    obj = res[0]; event = res[1] # can't use deconstruction because length of res can vary throughout implementations

                    if id(obj) == id(s):
                        if event == select.POLLIN:
                            cl, addr = s.accept()
                            cl.setblocking(0)
                            print('{}: client connected from {}'.format(self.__qualname__, addr))
                            poll.register(cl, READ_ONLY)
                            clientset.append(cl)
                        else:
                            raise Exception("Unhandled poll combo: {} {}".format(obj, event))
                    else:
                        cl = obj
                        try:
                            ret = self.__client(cl, event)
                        except OSError as e:
                            # TODO: Do we need to handle errno.ECONNRESET specially?
                            # TODO: seems like we might need to handle NameError? (or is my code just buggy?)
                            print("ERROR: Got", e)
                            sys.print_exception(e)

                        if ret == False:
                            print("{}: client {} disconnected".format(self.__qualname__, addr)) # DEBUG (remove later)
                            poll.unregister(cl)
                            clientset.remove(cl)
                            cl.close()
        finally:
            for cl in clientset:
                cl.close()
            s.close()

    def __client(self, cl, event):
        """Handles a single message from a client. Returns False for client
           disconnection, True otherwise.

        """

        # TODO: handle POLLERR?
        if event == select.POLLHUP:
            print("{},{}: got POLLHUP".format(self.__qualname__, cl)) # DEBUG
            return False

        def send_string(string):
            cl.send(bytes(string, 'ascii'))
            cl.send(b'\n')

        def send_error_msg(msg):
            print("ERROR:",msg)
            send_string("ERROR " + msg)

        line = cl.readline().decode('ascii') # TODO: don't decode bytestring for efficiency
        print("{},{}: got cmd '{}'".format(self.__qualname__, cl, line.rstrip())) # DEBUG (remove later)

        if not line or line == '\r\n' or line == '\n':
            return False
        if line[:6] == 'byebye': # TODO: us bytestring = memoryview
            send_string("CYA")
            return False

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
            send_string("OK " + self.vc.get_status_string())

        return True


class UDPVolumeServer(VolumeServer):
    """UDPVolumeServer implements a connectionless server protocol with
    textual commands with one command per datagram.

    Unique features: Will only reply with the state of the
    VolumeController when requested, in comparison with the TCP
    protocol. Clients will have to poll the server for its state. No
    confirmation is returned for normal commands.

    """
    def __init__(self):
        super().__init__()

    def server_loop(self, port, bindaddr="0.0.0.0"):
        # TODO: Implement me
        pass


class HTTPVolumeServer(VolumeServer):
    """ Why not? """

    def __init__(self):
        super().__init__()

    def server_loop(self, port=8080, bindaddr="0.0.0.0"):
        # TODO: implement
        pass


def start_tcpserver(port=1128):
    server = TCPVolumeServer()
    return server.server_loop(port=port)

def start_udpserver(port=1182):
    server = UDPVolumeServer()
    return server.server_loop(port=port)

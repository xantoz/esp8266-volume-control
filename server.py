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

    def __init__(self, vc=None):
        self.vc = vc or VolumeController()

    def _cmd_set(self, chan, level):
        """Command to set a channel.
           Usage: set <chan> <0-99>"""
        schan, lr = self.vc.get_chan(chan)
        self.vc.set_volume(schan, lr, int(level))

    def _cmd_setmaster(self, level):
        """Command to set master level.
           Usage: setmaster <0-99>"""
        self.vc.set_master(int(level))

    def _cmd_mutechan(self, chan, state):
        """Command to mute/unmute a single channel
           Usage: mute <chan> <0/1>"""
        schan, lr = self.vc.get_chan(chan)
        self.vc.set_mute(schan, lr, bool(int(state)))

    def _cmd_inc(self, chan, step=1):
        schan, lr = self.vc.get_chan(chan)
        level = self.vc.get_volume(schan, lr)
        if level < self.vc.MAX_LEVEL:
            self.vc.set_volume(schan, lr, level + int(step))

    def _cmd_incmaster(self, step=1):
        level = self.vc.get_master()
        if level < self.vc.MAX_LEVEL:
            self.vc.set_master(level + int(step))

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
                       'setmaster': _cmd_setmaster,
                       'inc': _cmd_inc,
                       'incmaster': _cmd_incmaster,
                       'status': _cmd_status,
                       'mute': _cmd_mute,
                       'mutechan': _cmd_mutechan,
                       'reset': _cmd_reset}

    def process_cmd(self, line):
        line = line.strip()
        if line:
            banana = line.split()
            cmd, args = banana[0], banana[1:]
            self._dispatch_table[cmd](self, *args)

    def server_init(self, timeout=None):
        """Init the server.

        If timeout=None server_onestep will poll in blocking (regular)
        mode. If timeout=0 server_onestep will poll non-blocking (do
        not wait until there is some event to act upon, only act if
        there is one). If timeout > 0 server_onestep can timeout, and
        return without having taken any action.

        """
        # This function is meant to be overriden in subclasses
        pass

    def server_onestep(self):
        """Do one round of servery stuff (loop body). """
        # This function is meant to be overriden in subclasses
        pass

    def server_deinit(self):
        """Tie up any loose ends and close the server socket."""
        # This function is meant to be overriden in subclasses
        pass

    def server_loop(self):
        """Run a foreground, blocking, server loop"""

        self.server_init(timeout=None)

        try:
            while True:
                self.server_onestep()
        finally:
            self.server_deinit()


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

    def __init__(self, port, bindaddr="0.0.0.0", client_timeout=5.0):
        """Create a TCPVolumeServer bound to port and bindaddr. client_timeout
           is the amount of seconds to wait in client connections
           before we deem the connection dead. Since client
           communication is blocking (but listening for connecting
           clients isn't) this needs to be a non-zero floating point
           value, so that the server loop cannot be stalled by an
           unresponsive client.
        """
        super().__init__()
        self.port = port
        self.bindaddr = bindaddr
        self.client_timeout = client_timeout

    def server_init(self, timeout=None):
        self.timeout = timeout
        addr = socket.getaddrinfo(self.bindaddr, self.port)[0][-1]
        self.s = socket.socket()
        self.s.setblocking(False)        # non-blocking because we use polling

        self.poll = select.poll()
        self.poll.register(self.s, READ_ONLY)
        # Keep a set of all clients so we can disconnect them properly
        # in case of a fatal error. We can't use set() because sockets
        # aren't hashable.
        self.clientset = []

        self.s.bind(addr)
        self.s.listen(5)

        print("{}: listening on {} (timeout={})".format(self.__qualname__, addr, self.timeout))

    def server_onestep(self):
        for res in self.poll.poll(self.timeout):
            #print("{}: poll: '{}'".format(self.__qualname__, res)) # DEBUG
            obj = res[0]; event = res[1] # can't use deconstruction because length of res can vary throughout implementations

            if id(obj) == id(self.s):
                if event == select.POLLIN:
                    cl, addr = self.s.accept()
                    self.__add_client(cl, addr)
                else:
                    raise Exception("Unhandled poll combo: {} {}".format(obj, event))
            else:
                cl = obj
                try:
                    ret = self.__client(cl, event)
                    if ret == False:
                        print("{}: client {} disconnected".format(self.__qualname__, cl)) # DEBUG (remove later)
                        self.__remove_client(cl)
                except OSError as e:
                    # TODO: Do we need to handle errno.ECONNRESET specially?
                    # TODO: seems like we might need to handle NameError? (or is my code just buggy?)
                    if e.errno == errno.ETIMEDOUT:
                        print("ERROR: communication with client {} timed out. Killing client.".format(cl))
                        sys.print_exception(e)
                        self.__remove_client(cl)
                    else:
                        print("ERROR: Got", e)
                        sys.print_exception(e)

    def server_deinit(self):
        for cl in self.clientset:
            cl.close()
        self.s.close()

        self.clientset = None
        self.s = None
        self.poll = None

    def __add_client(self, cl, addr):
        """Handles accepting a new client"""
        # TODO: store addr (associated with cl) for nicer debug printouts etc
        cl.setblocking(True) # Needed for reliable writing?
        if sys.platform != 'linux':
            # linux port of micropython doesn't support settimeout,
            # but we want to be able to run the server on linux when debugging
            cl.settimeout(self.client_timeout)
        print('{}: client connected from {}'.format(self.__qualname__, addr))
        self.poll.register(cl, READ_ONLY)
        self.clientset.append(cl)

    def __remove_client(self, cl):
        """Handles when a client disconnects"""
        self.poll.unregister(cl)
        self.clientset.remove(cl)
        cl.close()

    def __client(self, cl, event):
        """Handles a single message (receive, execute, reply) from a client.
           Returns False for client disconnection, True otherwise.
        """

        # TODO: handle POLLERR?
        if event == select.POLLHUP:
            print("{},{}: got POLLHUP".format(self.__qualname__, cl)) # DEBUG
            return False

        def send_string(string):
            val = bytearray(string)
            val.extend(b'\n')
            cl.write(val)

        def send_error_msg(msg):
            print("ERROR:",msg)
            send_string("ERROR " + msg)

        line = cl.readline().decode('ascii') # TODO: don't decode bytestring for efficiency
        print("{},{}: got cmd '{}'".format(self.__qualname__, cl, line.rstrip())) # DEBUG (remove later)

        if not line or line == '\r\n' or line == '\n':
            return False
        if line[:6] == 'byebye': # TODO: use bytestring + memoryview
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
            send_string("OK " + self.vc.get_status_string()) # TODO: inform all clients of the state change

        return True


class UDPVolumeServer(VolumeServer):
    """UDPVolumeServer implements a connectionless server protocol of
    textual commands with one command sent per datagram.

    Unique features: Will only reply with the state of the
    VolumeController when requested, in comparison with the TCP
    protocol. Clients will have to poll the server for its state to
    keep up to date. No confirmation is returned for normal commands.

    """
    def __init__(self, port, bindaddr="0.0.0.0"):
        super().__init__()
        self.port = port
        self.bindaddr = bindaddr

    def server_init(self, timeout=None):
        """Init the server."""

        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        addr = socket.getaddrinfo(self.bindaddr, self.port)[0][-1]
        self.s.bind(addr)
        if sys.platform != 'linux':
            # linux port of micropython doesn't support settimeout,
            # but we want to be able to run the server on linux when debugging
            self.s.settimeout(timeout) # set blocking/timeout mode

        print("{}: bound UDP socket to {}".format(self.__qualname__, addr)) # DEBUG

    def server_onestep(self):
        data, addr = self.s.recvfrom(256)
        print("{}: received {} from {}".format(self.__qualname__, repr(data), addr))

        def send_string(string):
            self.s.sendto(bytes(string, 'ascii'), addr)

        def send_error_msg(msg):
            print("ERROR:",msg)
            send_string("ERROR " + msg)

        # Notably the UDP protocol only replies if a command fails
        # (useful when debugging a faulty client) or if a status
        # message has been explicitly requested.

        try:
            self.process_cmd(data.decode('ascii'))
        except TypeError as e:
            send_error_msg("wrong amount of args")
            sys.print_exception(e)
        except KeyError as e:
            send_error_msg("no such command")
            sys.print_exception(e)
        except ValueError as e:
            send_error_msg("bad argument: " + str(e))
            sys.print_exception(e)

        if "status" in data:
            send_string("OK " + self.vc.get_status_string())

    def server_deinit(self):
        self.s.close()


class HTTPVolumeServer(VolumeServer):
    """ Why not? """

    def __init__(self, port=8080, bindaddr="0.0.0.0"):
        super().__init__()
        self.port = port
        self.bindaddr = bindaddr

    def server_init(self, timeout=None):
        # TODO: implement
        pass

    def server_onestep(self):
        pass

    def server_deinit(self):
        pass


def start_tcpserver(port=1128):
    server = TCPVolumeServer(port=port)
    return server.server_loop()

def start_udpserver(port=1182):
    server = UDPVolumeServer(port=port)
    return server.server_loop()

def start_httpserver(port=8080):
    server = HTTPVolumeServer(port=port)
    return server.server_loop()

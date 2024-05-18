#!/usr/bin/env python3

"""
Communications relay program for use with Elkulator I/O port support.

Copyright (C) 2022, 2024 Paul Boddie <paul@boddie.org.uk>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

from collections import deque
from os import read, remove, O_NONBLOCK
from os.path import exists, isfile
from socket import socket, AF_UNIX, SOCK_STREAM
from telnetlib import Telnet
import select, tempfile
import sys

# Python 2/3 compatibility.

def print_to(f, s):
    f.write(s)
    f.write("\n")

EMPTY = b""
CR = b"\r"
LF = b"\n"

stdin = sys.stdin
stdout = sys.stdout

if hasattr(stdout, "buffer"):
    stdout = stdout.buffer

# Conveniences.

class Channel:

    """
    A simple file-like object for direct stream access, exposing the file number
    for monitoring purposes, and providing a convenience method for writing.
    """

    def __init__(self, inp, outp, converter):
        self.inp = inp
        self.outp = outp
        self.converter = converter

    def fileno(self):
        return self.inp.fileno()

    def read(self, num):
        return read(self.fileno(), num)

    def write(self, s):
        self.outp.write(self.converter(s))

        if hasattr(self.outp, "flush"):
            self.outp.flush()

# Line ending conversion functions.

def cr_to_lf(s):

    """
    Convert output from the Electron having optional line feeds to data
    containing line feeds, replacing carriage returns.
    """

    return s.replace(LF, EMPTY).replace(CR, LF)

def lf_to_cr(s):

    """
    Handle input to the Electron having optional carriage returns to data
    containing carriage returns, replacing line feeds.
    """

    return s.replace(CR, EMPTY).replace(LF, CR)

def null(s):

    "A null conversion."

    return s

# Communications functions.

def session(poller, channels):

    "Use 'poller' to monitor the given 'channels'."

    # Map input descriptors to input and output channels.

    channel_map = {}
    for reader, writer in channels:
        channel_map[reader.fileno()] = reader, writer

    while 1:
        fds = poller.poll()
        for fd, status in fds:
            if status & (select.POLLHUP | select.POLLNVAL | select.POLLERR):
                print_to(sys.stderr, "Connection closed.")
                return
            elif status & select.POLLIN:
                reader, writer = channel_map[fd]
                s = reader.read(1)
                writer.write(s)


help_text = """\

Usage: %s <mode> [ <options> ] [ <filename> ]

Modes:

--console                   Interact using standard input and output
--telnet <host> [ <port> ]  Connect to, relay data to and from, the given host
"""

def main():

    "Initialise a server socket and open a session."

    # Obtain operating mode.

    args = deque(sys.argv)
    args.popleft()

    try:
        mode = args.popleft()
    except IndexError:
        mode = "--console"

    # Handle supported and unsupported modes.

    not_mode = not mode.startswith("--")

    if mode == "--console" or not_mode:

        # Push any possible filename back into the arguments.

        if not_mode:
            args.appendleft(mode)

        print_to(sys.stderr, "Starting console using standard input and output...")

    elif mode == "--telnet":
        host = "localhost"
        port = 23

        try:
            host = args.popleft()
            port = args.popleft()
        except IndexError:
            pass

        print_to(sys.stderr, "Connecting to telnet address %s:%s..." % (host, port))

    elif mode == "--help":
        print_to(sys.stderr, help_text % sys.argv[0])
        sys.exit(1)

    else:
        print_to(sys.stderr, "Mode not recognised:", mode)
        print_to(sys.stderr, help_text % sys.argv[0])
        sys.exit(1)

    # Obtain the communications socket filename and remove any previous socket file.

    try:
        filename = args.popleft()
        temporary_file = False
    except IndexError:
        filename = tempfile.mktemp()
        temporary_file = True

    if exists(filename) and not isfile(filename):
        remove(filename)

    # Obtain a socket and bind it to the given filename.

    s = socket(AF_UNIX, SOCK_STREAM)
    s.bind(filename)
    s.listen(0)

    # Accept a connection.

    print_to(sys.stderr, "Waiting for connection at: %s" % filename)

    c, addr = s.accept()

    print_to(sys.stderr, "Connection accepted.")

    # Employ file-like objects for reading and writing.

    reader = c.makefile("rb", 0)
    writer = c.makefile("wb", 0)

    # Obtain the relay destination and define the local and remote channels.

    if mode == "--telnet":
        server = Telnet(host, port)
        remote = Channel(server, server, null)
        local = Channel(reader, writer, null)
    else:
        server = None
        remote = Channel(stdin, stdout, cr_to_lf)
        local = Channel(reader, writer, lf_to_cr)

    # Employ polling on the socket input and on a telnet connection.

    poller = select.poll()
    poller.register(local.fileno(), select.POLLIN | select.POLLHUP | select.POLLNVAL | select.POLLERR)
    poller.register(remote.fileno(), select.POLLIN | select.POLLHUP | select.POLLNVAL | select.POLLERR)

    # Define the channel in each direction.

    channels = [(local, remote), (remote, local)]

    # Initiate a session.

    try:
        session(poller, channels)

    # Remove any temporary file.

    finally:
        if temporary_file:
            remove(filename)
        if server:
            server.close()

# Main program.

if __name__ == "__main__":
    main()

# vim: tabstop=4 expandtab shiftwidth=4

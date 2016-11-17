#!/usr/bin/env python

"""Ninja frontend interface.

This module implements a Ninja frontend interface that delegates handling each
message to a handler object
"""

import msgpack
import os
import select

HEADER = 0x4e4a5331 # NJS1
TOTAL_EDGES = 0
BUILD_STARTED = 1
BUILD_FINISHED = 2
EDGE_STARTED = 3
EDGE_FINISHED = 4
NINJA_INFO = 5
NINJA_WARNING = 6
NINJA_ERROR = 7

class Handler(object):
    """Empty Handler class

    This class implements empty methods for all messages parsed by the Frontend
    class.  Handler objects passed to Frontend may either subclass this class,
    or reimplement only the subset of methods they need.
    """

    def __init__(self):
        pass

    def update_total_edges(self, msg):
        """Update total edges handler.

        Called for a "total edges" message from Ninja.

        Args:
            msg (TotalEdges): Message parsed into a TotalEdges object.
        """
        pass

    def build_started(self, msg):
        """Build started handler.

        Called for a "build started" message from Ninja.

        Args:
            msg (BuildStarted): Message parsed into a BuildStarted object.
        """
        pass

    def build_finished(self, msg):
        """Build finished handler.

        Called for a "build finished" message from Ninja.

        Args:
            msg (BuildFinished): Message parsed into a BuildFinished object.
        """
        pass

    def edge_started(self, msg):
        """Edge started handler.

        Called for an "edge started" message from Ninja.

        Args:
            msg (EdgeStarted): Message parsed into an EdgeStarted object.
        """
        pass

    def edge_finished(self, msg):
        """Edge finished handler.

        Called for an "edge finished" message from Ninja.

        Args:
            msg (EdgeFinished): Message parsed into an EdgeFinished object.
        """
        pass

    def info(self, msg):
        """Info message handler.

        Called for an "info" message from Ninja.

        Args:
            msg (NinjaInfo): Message parsed into a NinjaInfo object.
        """
        pass

    def warning(self, msg):
        """Warning message handler.

        Called for a "warning" message from Ninja.

        Args:
            msg (NinjaWarning): Message parsed into a NinjaWarning object.
        """
        pass

    def error(self, msg):
        """Error message handler

        Called for an "error" message from Ninja.

        Args:
            msg (NinjaError): Message parsed into a NinjaError object.
        """
        pass

    def unknown(self, msg):
        """Unknown message handler.

        Called for an unknown message from Ninja.

        Args:
            msg: The raw unpacked message.
        """
        pass

class TotalEdges(object):
    """Parsed total edges message from Ninja.

    Attributes:
        total_edges (int): New value for total edges in the build.
    """

    def __init__(self, msg):
        assert len(msg) >= 2
        self.total_edges = msg[1]

class BuildStarted(object):
    """Parsed build started message from Ninja.

    Attributes:
        parallelism (int): Number of jobs Ninja will run in parallel.
        verbose (bool): Verbose value passed to Ninja.
    """

    def __init__(self, msg):
        assert len(msg) >= 3
        self.parallelism = msg[1]
        self.verbose = msg[2]

class BuildFinished(object):
    """Parsed build finished message from Ninja.

    Attributes:
    """

    def __init__(self, msg):
        assert len(msg) >= 1

class EdgeStarted(object):
    """Parsed edge started message from Ninja.

    Attributes:
        id (int): Edge identification number, unique to a Ninja run.
        start_time_millis (int): Edge start time in milliseconds since Ninja
            started.
        inputs (:obj:`list` of :obj:`str`): List of edge inputs.
        outputs (:obj:`list` of :obj:`str`): List of edge outputs.
        description (:obj:`str`): Description.
        command (:obj:`str`): Command.
        console (bool): Edge uses console.
    """

    def __init__(self, msg):
        assert len(msg) >= 7
        self.id = msg[1]
        self.start_time_millis = msg[2]
        self.inputs = msg[3]
        self.outputs = msg[4]
        self.description = msg[5]
        self.command = msg[6]
        self.console = msg[7]

class EdgeFinished(object):
    """Parsed edge finished message from Ninja.

    Attributes:
        id (int): Edge identification number, unique to a Ninja run.
        end_time_millis (int): Edge end time in milliseconds since Ninja
            started.
        status (int): Exit status (0 for success).
        output (:obj:`str`): Edge output, may contain ANSI codes .
        edge_started (:obj:`EdgeStarted`): EdgeStarted object with for the
            edge identification number.
    """
    def __init__(self, msg):
        assert len(msg) >= 5
        self.id = msg[1]
        self.end_time_millis = msg[2]
        self.status = msg[3]
        self.output = msg[4]
        self.edge_started = None

class Message(object):
    """Parsed text message from Ninja.

    Attributes:
        text (:obj:`str`): Text message from ninja.
    """
    def __init__(self, msg):
        assert len(msg) >= 2
        self.text = msg[1]

class NonBlockingReader(object):
    """Iterator class that returns data read from an file descriptor without
    blocking.

    Args:
        fd (int): File descriptor number.
    """
    def __init__(self, fd):
        self.fd = fd
        self.poll = select.poll()
        self.poll.register(self.fd, select.POLLIN|select.POLLERR|select.POLLHUP)

    def __iter__(self):
        return self

    def next(self):
        while True:
            ready = self.poll.poll()
            if len(ready) > 0:
                data = os.read(3, 1024**2)
                if len(data) > 0:
                    return data
                else:
                    raise StopIteration

class InvalidMessageException(Exception):
    """Exception thrown when Frontend fails to parse a message

    Args:
        error (str): Human readable string describing the exception
        msg (str): Unpacked MessagePack message

    Attributes:
        error (str): Human readable string describing the exception
        msg (str): Unpacked MessagePack message
    """
    def __init__(self, error, msg):
        self.error = error
        self.msg = msg
        Exception.__init__(self)

class Frontend(object):
    """Class that parses Ninja messages and delegates handling to a Handler
    object.

    Args:
        handler (:obj:`Handler`): Handler object that will handle incoming
            messages.

    Attributes:
        running (:obj:`dict` of :obj:`EdgeStarted` messages): edges that
            have been started but not yet finished.
    """
    def __init__(self, handler):
        self.handler = handler
        self.unpacker = msgpack.Unpacker(encoding="utf-8")
        self.running = {}
        self.seen_header = False
        self.fd = 3
        self.reader = NonBlockingReader(self.fd)

    def run(self):
        """Read data from self.fd and pass to the unpacker and handler.

        Runs until the other end of the file descriptor is closed.
        """
        for data in self.reader:
            self.handle(data)

    def handle(self, data):
        """Handle a block of incoming data.  Feeds it to the unpacker,
        then handles any completed messages.  Incomplete messages will be
        buffered and handled on a future call to handle.

        Args:
            data (:obj:`str`): block of incoming data
        """
        self.unpacker.feed(data)
        for msg in self.unpacker:
            if not self.seen_header:
                if type(msg) is not int:
                    raise InvalidMessageException("Expected int, got " + str(type(msg)), msg)
                if msg != HEADER:
                    raise InvalidMessageException("Expected {}, got {}".format(BUILD_STARTED, msg), msg)
                self.seen_header = True
                continue

            if type(msg) is not list:
                raise InvalidMessageException("Expected list, got " + str(type(msg)), msg)
            if len(msg) < 1:
                raise InvalidMessageException("Expected at least one element in list", msg)

            msgtype = msg[0]
            func = None
            obj = None

            try:
                if msgtype == TOTAL_EDGES:
                    func = self.handler.update_total_edges
                    obj = TotalEdges(msg)
                elif msgtype == BUILD_STARTED:
                    func = self.handler.build_started
                    obj = BuildStarted(msg)
                elif msgtype == BUILD_FINISHED:
                    func = self.handler.build_finished
                    obj = BuildFinished(msg)
                elif msgtype == EDGE_STARTED:
                    obj = EdgeStarted(msg)
                    self.running[obj.id] = obj
                    func = self.handler.edge_started
                elif msgtype == EDGE_FINISHED:
                    func = self.handler.edge_finished
                    obj = EdgeFinished(msg)
                    obj.edge_started = self.running[obj.id]
                    del self.running[obj.id]
                elif msgtype == NINJA_INFO:
                    func = self.handler.info
                    obj = Message(msg)
                elif msgtype == NINJA_WARNING:
                    func = self.handler.warning
                    obj = Message(msg)
                elif msgtype == NINJA_ERROR:
                    func = self.handler.error
                    obj = Message(msg)
                else:
                    func = self.handler.unknown
                    obj = msg
            except AttributeError:
                continue
            func(obj)

Ninja Frontend Interface
========================

Ninja can use another program as a frontend to display build status information.
This document describes the interface between Ninja and the frontend.

Connecting
----------

The frontend is passed to Ninja using a --frontend argument.  The argument is
executed the same as a build rule Ninja, wrapped in `sh -c` on Linux.  The
frontend will be executed with the read end of a pipe open on file descriptor
`3`.

Ninja will pass [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) 
messages.  The first message will always be the bytes `0xce`, `0x4e`, `0x4a`, `0x53`, followed by a byte for the major version number.  This is a MessagePack 
unsigned integer, with the value for version 1.0 `0x4e4a5331`, which is ASCII 
characters `NJS1`.  The major version number will be updated whenever
incompatible changes are made to the format.  Frontends MUST reject streams that
start with an unknown major version number.

All remaining messages are a [MessagePack array](https://github.com/msgpack/msgpack/blob/master/spec.md#formats-array),
with an integer that specifies the type of the message as the first element
of the array.  Unless the major version number is changed, elements in the array
will never be removed, and new elements will only be added to the end.
Frontends SHOULD accept messages with more elements in the array than expected.
The frontend MAY require a minimum number of elements in the array.  Frontends
SHOULD accept messages with unknown types.

stdin/stdout/stderr
-------------------

The frontend will have stdin, stdout, and stderr connected to the same file
descriptors as Ninja.  The frontend MAY read from stdin, however, if it does,
it MUST NOT read from stdin whenever a job in the console pool is running,
from when an `edge started` message is received with the `use console` boolean
value set to `true`, to when an `edge finished` message is received with the
same value for `id`.  Console rules may write directly to the same stdout/stderr
as the frontend.

Exiting
-------

The frontend MUST exit when the input pipe on fd `3` is closed.  When a build
finishes, either successfully, due to error, or on interrupt, Ninja will close
the pipe and then block until the frontend exits.

Messages
--------

### Total edges

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `total edges` message type (`0`) |
| 1 | unsigned int | New value for total edges in the build |

v1.0 array length: 2

### Build started

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `build started` message type (`1`) |
| 1 | unsigned int | Number of jobs Ninja will run in parallel |
| 2 | boolean | Verbose value passed to Ninja |

v1.0 array length: 3

### Build finished

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `build finished` message type (`2`) |

v1.0 array length: 1

### Edge started

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `edge started` message type (`3`) |
| 1 | unsigned int | Edge identification number, unique to a Ninja run |
| 2 | unsigned int | Edge start time in milliseconds since Ninja started |
| 3 | array of strings | List of edge inputs |
| 4 | array of strings | List of edge outputs |
| 5 | string | Description |
| 6 | string | Command |
| 7 | boolean | Edge uses console |

v1.0 array length: 8

### Edge finished

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `edge finished` message type (`4`) |
| 1 | unsigned int | Edge identification number, matching a previous `edge started` message |
| 2 | unsigned int | Edge end time in milliseconds since Ninja started |
| 3 | int | Exit status (0 for success ) |
| 4 | string | Edge output, may contain ANSI codes |

v1.0 array length: 5

### Info

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `info` message type (`5`) |
| 1 | string | Info message from Ninja |

v1.0 array length: 2

### Warning

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `warning` message type (`6`) |
| 1 | string | Warning message from Ninja |

v1.0 array length: 2

### Error

| Array Element | Type | Contents|
| --- | --- | --- |
| 0 | unsigned int | `error` message type (`7`) |
| 1 | string | Error message from Ninja |

v1.0 array length: 2

Experimenting with frontends
----------------------------

To run Ninja with a frontend that mimics the behavior of Ninja's normal output:
```
$ ./ninja --frontend=frontends/native.py
```

To save serialized output to a file:
```
$ ./ninja --frontend='cat /proc/self/fd/3 > ninja.msgpack' all
```

To run a frontend with serialized input from a file:
```
$ frontends/native.py 3< ninja.msgpack
```

The serialized output of a clean Ninja build is included in `frontend/ninja.msgpack`.


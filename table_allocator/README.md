# Table Allocator

Table Allocator consists of a client and a server, and is a tool for allocating
and distributing unique values based on some identifier. It is intended for use
when configuring routing on multihomed hosts
([see](https://github.com/kristrev/multihomed-routing/blob/master/README.md)),
as each interface (address) requires a separate table. The server will be
started at boot and run as a daemon, while the clients are started on-demand
(for example when a lease is acquired using DHCP). By default, the client will
move to the background once a lease (from the allocator server) is acquired. If
no lease is acquire, the client will exit. 

In both cases, a value will be written to stdout. If a lease has been acquired,
we will write the value received from the server. If no lease has been acquired,
we will write 0. The latter can be used as a signal to write multiple routes to
the main routing table (with different metrics). Each lease has a lifetime and
have to be refreshed, which is handled automatically by the client. Also, for
IPv4, the client creates and manages three routing rules that are required for
routing to be configured correctly.

If an address or link is deleted, the client will delete the rules, release the
lease and exit. If a lease has not been renewed or released, the server will
automatically remove the lease when it expires.

Communication between the client and server uses UNIX-domain sockets and
abstract names as addresses. The messages passed back and forth are
JSON-objects. For example, a request might looks like this:

`{"address":"192.168.5.113","ifname":"eth1","addr_family":2,"cmd":0,"version":1}`

While a reply looks like this:

`{"version":1,"cmd":2,"table":10000,"lease_expires":260903}`

## Server

The server is responsible for allocating and distributing the tables. It uses an
sqlite3 database for persistent storage, and a bitmap for quick lookups when
allocating/releasing tables. The server can be run as a normal user and supports
the following command line arguments:

* -c: path to configuration file, required.

The configuration file must contain one JSON object with the following keys:

* socket\_path: string, name of socket (required).
* table\_offset: int, value of first table (required).
* num\_tables: int, number of tables to allocate (required).
* table\_timeout: int, validity of lease (seconds, required).
* db\_path: string, path to leases database (required).
* do\_syslog: bool, write to syslog (optional, default false).
* log\_path": string, path to logfile (optional, default stderr)
* addr\_families: object, which address families to support. Valid keys are
  "inet", "inet6" and "unspec" and they key is a bool. At least one must be
  true.

See files/server\_config.json for a configuration example.

## Client

The client requests and maintains a lease, as well as the routing rules (v4,
soon v6). The client must be run as root in order to work correctly (or a user
with permissions to update routing). We support the following command line
arguments:

* -4: set address family to IPv4 (default is UNSPEC).
* -6: set address family to IPv6.
* -s/--syslog: enable logging to syslog (default off).
* -l/--log\_path: path to logfile (default stderr).
* -a/--address: address to allocate table for (required, a general identifier for UNSPEC).
* -n/--netmask: netmask for use with address (required with -4).
* -i/--ifname: interface to allocate table for (required).
* -t/--tag: optional tag to send to server.
* -r/--release: set command to release instead of request.
* -d/--destination: Path to server socket (required).
* -f/--foreground: Run application in foreground.
* -h/--help: this information.


## Building and installation

Table Allocator Client and Server are built using CMake:

* Enter either the client or server folder.
* mkdir build.
* cd build && cmake .. && make.
* If you want to compile and build a deb-package, replace the last command with
  make package.

You can also build the applications in parallel (does not work for package).
Follow the same steps as above, but make the build directory in the root folder
(i.e., .../table\_allocator).  The client requires libuv, libmnl and json-c,
while the server requires libuv, libsqlite3 and json-c.

The easiest way to install the applications, is to build and use the
deb-packages. If anyone wants to add support for other packet formats, then that
would be great. For examples of how to use the client, you can look at the
scripts located
[here](https://github.com/kristrev/multihomed-routing/tree/master/scripts).

## TODO

* Message authentication. Figure out if it is possible to get cmsg to play nice
  with libuv.

# Configuring routing on multihomed Linux-hosts

Linux supports multihoming out of the box. However, when a Linux&dash;device is
connected to multiple networks simultaneously, the kernel will often be unable
to make use of all the different networks due to overlapping routes (typically
the default routes). Configuring routing on multihomed hosts requires special
care, and is in most cases not handled properly by the default set of
networking/routing tools installed on a host. In this document, I will present
a solution that can be used to automatically configure routing correctly on
multihomed Linux hosts. The solution integrates with existing tools like
NetworkManager and ifupdown, so you will in most cases be able to use the tools
of your choice to configure the interface.

The main git-repository for the solution can be found [here](https://github.com/kristrev/multihomed-routing).

## TL;DR

* Install the table allocator client and server, potentially update server
configuration.

* Decide how you want to configure your interface (ifupdown, NetworkManager, or
something else).

* If you use ifupdown or NetworkManager, install either if-up-script of
dhclient-hook. If you use another tool for configuring the interfaces, you need to write a
script (contributions are welcome!). You can probably use the existing scripts as inspiration.

* If you use ifupdown, create stanzas for your interfaces.

* Read the section on DNS if you want fully functional, multihomed name
resolving.

* Start the interface(s).

## Motivation

The work presented here was supported by the EU-funded research-project
[MONROE](https://www.monroe-project.eu/), which is building a testbed
consisting of multihomed test nodes connected to different mobile broadband
provides, WAN and WIFI. Up until now, a tool called
[multi](https://github.com/kristrev/multi) has been used to configure routing
on the hosts. While multi has done its job well and been an invaluable tool in
getting MONROE up and running, it is starting to show its age and some of the
choices I made when I wrote the tool is starting to be a pain to deal with. The
tool was written by me during my PhD-work and since I was young(-ish), naive
and over-ambitious, multi became a hybrid between a network manager and routing
daemon. This resulted in stuff like implementing a small DHCP client inside the
tool. 

In order to support more advanced network configuration, IPv6, etc., we decided
on designing and building a new routing solution for the nodes. Instead of
trying to duplicate functionality available elsewhere, the new multihomed
routing solution builds on as many existing components as possible. Interfaces
will be configured by updating for example /etc/network/interfaces, and instead
of using multi for DHCP, etc., we will instead create routing configuration
scripts that will be run when a lease is acquired or interface goes down.

## Routing overview

When configuring routing on multihomed hosts there are two approaches to choose
from. One is to keep all routes in the same table and assign the routes
belonging to different interfaces different metrics. The second is to assign
each interface (address) a separate routing table, and then have a set of rules
for which traffic should result in lookups in which tables. This approach is
known as [policy based
routing](https://en.wikipedia.org/wiki/Policy-based_routing) and is what we
have based our configuration solution on. In addition to providing a degree of
separation between the different interfaces, policy based routing supports, as
the name implies, implementing more advanced routing policies. One can for
example configure the routing so that traffic from a container or application
(on recent kernels) only trigger lookups in one routing table. In MONROE, the
experiments are deployed as Docker-containers, and routing policies can be used
to "bind" one container to one operator.

With our solution, three rules are created per interface (address):

* The first rule (with priority 10000) is for traffic bound to a specific
address/interfaces (a bound socket for example). Each route contains both
interface and source address, so that routing will work even when there are
multiple interfaces with the same address present. The rules are on the format `from X/Y lookup 100`. The reason for including the netmask is so that routing
traffic through the node will work out of the box.

* The second rule (with priority 20000) is for traffic destined to a network.
These rules are required for routing traffic to the different networks
correctly, for example required when communicating with some other equipment
like a DNS server or router.

* The third rule (with priority 91000) is the match all rule, which is used to
route unbound traffic. The order of the 91000-rules depend on the order in
which interfaces came up, so there is no guarantee that the current route for
unbound traffic has internet connectivity. A separate tool which checks the
connections and maintains an unbound traffic rule (with a higher priority than 91000) is required if this guarantee is desirable.

On my machine, the rules looks as follows:

```
kristrev@kristrev-ThinkPad-X1-Carbon-2nd:~$ ip rule
0:	from all lookup local 
220:	from all lookup 220 
10000:	from 172.16.5.187 lookup 10001 
10000:	from 192.168.5.113 lookup 10000 
20000:	from all to 172.16.5.187/22 lookup 10001 
20000:	from all to 192.168.5.113/24 lookup 10000 
32766:	from all lookup main 
32767:	from all lookup default 
91000:	from all iif lo lookup 10001 
91000:	from all iif lo lookup 10000 
```

And a routing table:

```
kristrev@kristrev-ThinkPad-X1-Carbon-2nd:~$ ip ro show table 10000
default via 192.168.5.1 dev eth1 src 192.168.5.113 
192.168.5.0/24 dev eth1 scope link src 192.168.5.113 
```

## Configuring interfaces

Interfaces can in most cases be configured using your tool of choice (for
example ifupdown or through NetworkManager). If you already know how to
configure your interfaces, or just trust for example NetworkManager, you can
skip the rest of this section.

In order to save space on the MONROE-nodes we do not have NetworkManager
installed (and we don't need it), so we chose the ifupdown-approach. Static
interfaces are easy to handle, but on our hosts network interfaces are added
and removed while the system is running (for example USB modems). Fortunately,
/etc/network/interfaces supports "source" and "source-directory" stanzas (see
[man 5
interfaces](https://manpages.debian.org/jessie/ifupdown/interfaces.5.en.html)),
enabling the interface configuration to be split across multiple files and
directories. Dynamically adding and removing files works, any changes are
picked up by the ifup/ifdown tools. Our source-directory stanza is as follows:

`source-directory /tmp/network-conf`

Where to keep the additional configuration files and how and when to
create/remove them depends on the user-case and system configuration. In
MONROE, we store the configuration files in /var/run/network-conf and these
files are generated/removed when interfaces are connect/disconnect. We assume
that all dynamic interfaces will be configured using DHCP, so all files have
the following format:

`iface <ifname> inet dhcp`

## Table Allocator

Routing tables must be unique to each interface (address), and the role of the
Table Allocator is to distribute these tables. The Allocator consists of a
server and a client, and the code can be found
[here](https://github.com/kristrev/multihomed-routing/tree/master/table_allocator).
A client requests a table lease for a given address family (support for v4, v6
and unspec), interface and address. If there are tables available, the server
will reply with the routing table allocated to this client and the client will
refresh the lease at a given interval (currently half the lease time). If a
lease is not renewed or explicitly removed, then the server will automatically
removed it.

There is nothing really special going on inside these applications.
Communication goes over domain sockets (datagram) using abstract naming for
addresses (authentication is on the todo-list) and the messages are JSON
objects. The server uses sqlite3 for persistent lease storage and a bitmap for
quick allocation/release, and can as of now only handle one client at a time. A
single client at the time is fine for now, but is the first thing that should
be fixed if (when) scalability becomes an issue.

The client will set up the three routing rules mentioned above when a table has
been allocated for a v4 address (v6 is coming), and remove them (and release
the lease + exit) if either address or interface is removed. If the client
fails to get a lease, it will try five times (with a two second timeout between
retries) before giving up. When the client gets a table lease, it will by
default move to the background.

If no lease is acquired, the client will output 0 (as opposed to the allocated
routing table). All the configuration scripts (next section) will interpret
this as that routes should be added to the main routing table, with the
interface index as metric. This is a fallback to ensure that the device has a
working (albeit incorrect) routing configuration, and you should have a
watchdog or something checking for default routes in the main table and take
action (for example restart server).

## Configuring routing

Routing is configured using shell-scripts that are run when an address is
acquired/lost. Which script(s) to use depends on how you have chosen to
configure your network interfaces. We currently support ifupdown and
NetworkManager and our scripts are limited to IPv4 so far, but IPv6 is coming
very soon (contributions are always welcome).

### ifupdown

#### Static address

In order to configure routing for interfaces with a static address, we have
created an ifup-script (found
[here](https://github.com/kristrev/multihomed-routing/blob/master/scripts/interfaces-static/multihomed-routing-ifup)).
This script must be installed in `/etc/network/if-up.d/` (at least on
Debian/Ubuntu) and will be run every time an interface with a static IP has
been set as up. Technically it will run for every interface that is set as up,
but guards are in place so that unless the script is called with a static
address or from NetworkManager (more about that later) it will exit immediately.

When the script is run, addresses, etc. is already assigned to the interface and
routes are configured. Thus, we need to request a table from the Table
Allocator and move the already added routes to the correct table. This is done
in four commands, two `ip -4 ro del` and two `ip -4 ro add`. Since the Table
Allocator Client is responsible for managing the rules and routes are deleted
automatically when an interface is removed/we run ifdown, no
if-down.d-script is needed.

#### DHCP

Our script for use when DHCP (using dhclient) is used is implemented as a dhclient enter hook. The script can be found
[here](https://github.com/kristrev/multihomed-routing/blob/master/scripts/dhclient/multihomed-routing.sh)
and must be installed to /etc/dhcp/dhclient-enter-hooks.d/multihomed-routing
(note the .sh has to be removed). We handle the *BOUND*, *RENEW*, *REBIND*,
*REBOOT*, *TIMEOUT*, *EXPIRE*, *FAIL*, *RELEASE* and *STOP*-states. In the five
first states, we configure routing much the same way as in the script used for
static addresses (most of the complexity comes from handling the different DHCP
states) and update our resolv-file (more about that later). In the last three
states, we clean up the resolv-file.

### NetworkManager

NetworkManager will, via. one of its dispatcher scripts
(/etc/NetworkManager/dispatcher.d/01ifupdown on my Ubuntu-machine), run the
scripts in if-up.d (and if-post-down.d). We have added a NetworkManager-path to
the script mentioned under "ifupdown - Static address", so if you install this
script then routing will be configured correctly when NetworkManager is used.

Note that NetworkManager has its own algorithm(s) for determining which
interface should be the default interface, by setting different metrics on the
default routes (and other routes) added to the main routing table. We respect
these metrics and do not delete any routes from the main routing table. The
91000-rules mentioned has in other words no effect when NetworkManager is used.

## DNS

Using the default resolver and code for creating/updating resolv-conf, DNS
requests will either be sent to one or all servers, and you have no control
over which interface/address is used to communicate with a server. If for
example a server has an address outisde the network of the interface, then the
default route will be used. If the default route is over a different interface
than the one that acquired the server, then requests will in many cases be
dropped since they have a different source address than what the server
expects.

Fortunately, [dnsmasq](http://www.thekelleys.org.uk/dnsmasq/doc.html) supports
specifying which interface and address to be used to communicate with a server.
If you want to set up DNS to work correctly, you can do
the following:

* Install dnsmasq and configure it to be the default resolver. The steps for
Debian can be found [here](https://wiki.debian.org/HowTo/dnsmasq).

* We will keep our config in a separate config file. Uncomment the last line in
/etc/dnsmasq.conf, so that /etc/dnsmasq.d/\*.conf is included when dnsmasq
starts.

* Copy the
[following](https://github.com/kristrev/multihomed-routing/blob/master/config/dnsmasq/dnsmasq-custom.conf)
file into /etc/dnsmasq.d/. You can of course call it something else.

* We will write the DNS servers to a special dnsmasq-file, known as the servers
file. Update the desired path to this file in the configuration file you just
copied (*servers-file*).

* So far, we only write servers that we acquire when dhclient is used. If you
change the path of the servers file, update the dhclient-hook
(*DNSMASQ_SERVER_PATH*-variable).

When an interface comes up or goes down, the servers-file is update. We also
signal dnsmasq (*SIGHUP*), which causes the application to reload the list of
available DNS servers.

## Misc.

### ifupdown and hotplug

ifupdown does not deal very well with hotplugging. ifup is easy to solve, you for
example have a tool which listens for USB connect events and does ifup or a
udev rule. ifdown, on the other hand, is harder. If you try to do ifdown on a
non-existent interface, you will just be told that ifdown will ignore the
interface (it is unknown). However, for example dhclient and proper clean-up
will not be performed. In order to work around this issue, we created a udev
rule which runs a script when a *remove* event happens. The script stops
dhclient for that interface and removes any entry from the dnsmasq
servers-file. Script and rule can be found
[here](https://github.com/kristrev/multihomed-routing/tree/master/udev).

## Summary

By following this document, installing the tools, scripts and configuring them
when needed, you should have a system which will automatically configure
multihomed routing correctly. If you have any comments, questions or
contributions, you can either send me an
[email](mailto:kristian.evensen@gmail.com) or create an issue/PR.

This work was was funded by the H2020-project [MONROE](https://www.monroe-project.eu/).

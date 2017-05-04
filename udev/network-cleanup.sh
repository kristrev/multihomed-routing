#!/bin/sh

DNSMASQ_SERVER_PATH="/tmp/dnsmasq-servers.conf"

#kill all dhclient processes
DHCLIENT_PIDS=$(pgrep -f "dhclient.*$1" | tr "\n" " ")

for pid in $DHCLIENT_PIDS;
do
    kill -9 $pid
done

IFACE_SERVER_MATCH="@$1"
grep "$IFACE_SERVER_MATCH" "$DNSMASQ_SERVER_PATH"

if [ $? -eq 0 ];
then
    sed -i "/$IFACE_SERVER_MATCH/d" "$DNSMASQ_SERVER_PATH"
    kill -s HUP $(pgrep dnsmasq)
fi

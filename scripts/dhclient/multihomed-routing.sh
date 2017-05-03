#!/bin/sh

#Install this script by copying it to:
#/etc/dhcp/dhclient-enter-hooks.d/multihomed-routing (note that .sh is removed)

ADDR_RULE_PREF=10000
NW_RULE_PREF=20000
LO_RULE_PREF=91000
DNSMASQ_SERVER_PATH="/tmp/dnsmasq-servers.conf"

make_resolv_conf()
{
    if [ -z "$new_domain_name_servers" ]; then
        return;
    fi

    for nameserver in $new_domain_name_servers; do
        server_str="server=$nameserver@$new_ip_address@$interface"

        grep "$server_str" "$DNSMASQ_SERVER_PATH"

        if [ $? -ne 0 ]; then
            echo "$server_str" >> "$DNSMASQ_SERVER_PATH"
        fi
    done

    kill -s HUP $(pgrep dnsmasq)
}

delete_from_resolv_conf()
{
    match="$old_ip_address@$interface"

    sed -i "/$match/d" "$DNSMASQ_SERVER_PATH"

    kill -s HUP $(pgrep dnsmasq)
}

update_addr_route()
{
    rt_table=$1
    if_metric=$2

    ip -4 ro delete \
        ${new_network_number}${new_subnet_mask:+/$new_subnet_mask} \
        dev ${interface}
    ip -4 ro add \
        ${new_network_number}${new_subnet_mask:+/$new_subnet_mask} \
        dev ${interface} src ${new_ip_address} ${if_metric:+metric $if_metric} \
        table ${rt_table}
}

handle_bound_renew_rebind_reboot()
{
    set_hostname

    #we ignore alias for now
    if [ -n "$old_ip_address" ] && [ -n "$alias_ip_address" ] &&
       [ "$alias_ip_address" != "$old_ip_address" ]; then
            # alias IP may have changed => flush it
            ip -4 addr flush dev ${interface} label ${interface}:0
    fi

    if [ -n "$old_ip_address" ] &&
       [ "$old_ip_address" != "$new_ip_address" ]; then
            # leased IP has changed => flush it
            ip -4 addr flush dev ${interface} label ${interface}
    fi

    if [ -z "$old_ip_address" ] ||
       [ "$old_ip_address" != "$new_ip_address" ] ||
       [ "$reason" = "BOUND" ] || [ "$reason" = "REBOOT" ]; then
        # new IP has been leased or leased IP changed => set it
        ip -4 addr add ${new_ip_address}${new_subnet_mask:+/$new_subnet_mask} \
            ${new_broadcast_address:+broadcast $new_broadcast_address} \
            dev ${interface} label ${interface}

        rt_table=$(/usr/sbin/table_allocator_client -4 -s -a "$new_ip_address" -n "$new_subnet_mask" -i "$interface" -d tas_socket)

        #Use default table and if_idx as metric if table_allocator_client can't
        #get a lease. This is not ideal, but will ensure that the device has
        #working routing
        if [ "$rt_table" -eq 0 ];
        then
            rt_table=254;
            if_idx=$(/sbin/ip link show dev "$interface" | head -1 | cut -d " " -f 1 | cut -d ":" -f 1)
        fi

        #remove default address route and add it to the correct table
        update_addr_route $rt_table $if_idx

        if [ -n "$new_interface_mtu" ]; then
            # set MTU
            ip link set dev ${interface} mtu ${new_interface_mtu}
        fi

	    # if we have $new_rfc3442_classless_static_routes then we have to
	    # ignore $new_routers entirely
	    if [ ! "$new_rfc3442_classless_static_routes" ]; then
		    # set if_metric if IF_METRIC is set or there's more than one router
		    if_metric="$IF_METRIC"
		    if [ "${new_routers%% *}" != "${new_routers}" ]; then
			if_metric=${if_metric:-1}
		    fi

		    for router in $new_routers; do
			if [ "$new_subnet_mask" = "255.255.255.255" ]; then
			    # point-to-point connection => set explicit route
			    ip -4 ro add ${router} dev $interface src ${new_ip_address} \
                    table ${rt_rable} >/dev/null 2>&1
			fi

            #do not override default IF_METRIC
            if [ "$rt_table" -eq 254 -a -z "$IF_METRIC"];
            then
                if_metric="$if_idx"
            fi

			# set default route
            ip -4 ro add default via ${router} dev ${interface} \
                ${if_metric:+metric $if_metric} src ${new_ip_address} \
                table ${rt_table} >/dev/null 2>&1

			if [ -n "$if_metric" ]; then
			    if_metric=$((if_metric+1))
			fi
		    done
	    fi
    fi

    #we currently don't do anything with alias addresses
    if [ -n "$alias_ip_address" ] &&
       [ "$new_ip_address" != "$alias_ip_address" ]; then
        # separate alias IP given, which may have changed
        # => flush it, set it & add host route to it
        ip -4 addr flush dev ${interface} label ${interface}:0
        ip -4 addr add ${alias_ip_address}${alias_subnet_mask:+/$alias_subnet_mask} \
            dev ${interface} label ${interface}:0
        ip -4 route add ${alias_ip_address} dev ${interface} >/dev/null 2>&1
    fi

    # update /etc/resolv.conf
    make_resolv_conf

    #This function replaces the handling in the main script. Thus, we need to
    #prevent the main script from handling these four states
    unset reason
}

handle_timeout()
{
    #algorithm for timeout handling should be as follows:
    # * Send an allocation request for this interface, address and address
    # family.
    # * Try to configure everything as usual. If adress etc. is already
    # configured, the different steps will just fail.
    #There is no chance that the routing table for one interface + address will
    #move, since we control which table to use

    #todo: need to allocate/read table here
    rt_table=$(/usr/sbin/table_allocator_client -4 -s -a "$new_ip_address" -n "$new_subnet_mask" -i "$interface" -d tas_socket)
    if [ "$rt_table" -eq 0 ];
    then
        rt_table=254;
        if_idx=$(/sbin/ip link show dev eth0 | head -1 | cut -d " " -f 1 | cut -d ":" -f 1)
    fi

    #as always, we don't care about alias
    if [ -n "$alias_ip_address" ]; then
        # flush alias IP
        ip -4 addr flush dev ${interface} label ${interface}:0
    fi

    # set IP from recorded lease
    ip -4 addr add ${new_ip_address}${new_subnet_mask:+/$new_subnet_mask} \
        ${new_broadcast_address:+broadcast $new_broadcast_address} \
        dev ${interface} label ${interface}

    # move adress route to correct table
    update_addr_route $rt_table $if_idx

    if [ -n "$new_interface_mtu" ]; then
        # set MTU
        ip link set dev ${interface} mtu ${new_interface_mtu}
    fi

    # if there is no router recorded in the lease or the 1st router answers pings
    if [ -z "$new_routers" ] || ping -q -c 1 "${new_routers%% *}"; then
        # if we have $new_rfc3442_classless_static_routes then we have to
        # ignore $new_routers entirely
        if [ ! "$new_rfc3442_classless_static_routes" ]; then
            if [ -n "$alias_ip_address" ] &&
               [ "$new_ip_address" != "$alias_ip_address" ]; then
            # separate alias IP given => set up the alias IP & add host route to it
            ip -4 addr add ${alias_ip_address}${alias_subnet_mask:+/$alias_subnet_mask} \
                dev ${interface} label ${interface}:0
            ip -4 route add ${alias_ip_address} dev ${interface} >/dev/null 2>&1
            fi

            # set if_metric if IF_METRIC is set or there's more than one router
            if_metric="$IF_METRIC"
            if [ "${new_routers%% *}" != "${new_routers}" ]; then
            if_metric=${if_metric:-1}
            fi

            # set default route
            for router in $new_routers; do
                if [ "$rt_table" -eq 254 -a -z "$IF_METRIC"];
                then
                    if_metric="$if_idx"
                fi

                ip -4 ro add default via ${router} dev ${interface} \
                    ${if_metric:+metric $if_metric} src ${new_ip_address} \
                    table ${rt_table} >/dev/null 2>&1

                if [ -n "$if_metric" ]; then
                    if_metric=$((if_metric+1))
                fi
            done
        fi

        # update /etc/resolv.conf
        make_resolv_conf
    else
        # flush all IPs from interface
        ip -4 addr flush dev ${interface}
        exit_with_hooks 2
    fi

    unset reason
}

case "$reason" in
    BOUND|RENEW|REBIND|REBOOT)
        handle_bound_renew_rebind_reboot
        ;;
    TIMEOUT)
        handle_timeout
        ;;
    EXPIRE|FAIL|RELEASE|STOP)
        delete_from_resolv_conf
        ;;
    *)
        return;
        ;;
esac


#!/bin/sh

PARAM=$*
# Add paramaters equivalent to those passed for up command
[ -z "$PARAM" ] && PARAM="$dev $tun_mtu $link_mtu $ifconfig_local $ifconfig_remote"

my_logger(){
	if [ "$VPN_LOGGING" -gt "3" ]
	then
		/usr/bin/logger -t "openvpn-routing" "$1"
	fi
}


create_client_list(){
	OLDIFS=$IFS
	IFS="<"

	for ENTRY in $VPN_IP_LIST
	do
		[ -z "$ENTRY" ] && continue
		TARGET_ROUTE=$(echo $ENTRY | cut -d ">" -f 4)
		if [ "$TARGET_ROUTE" = "WAN" ]
		then
			TARGET_LOOKUP="main"
			WAN_PRIO=$((WAN_PRIO+1))
			RULE_PRIO=$WAN_PRIO
			TARGET_NAME="WAN"
		else
			TARGET_LOOKUP=$VPN_TBL
			VPN_PRIO=$((VPN_PRIO+1))
			RULE_PRIO=$VPN_PRIO
			TARGET_NAME="VPN client "$VPN_UNIT
		fi
		VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
		if [ "$VPN_IP" != "0.0.0.0" ] && [ -n "$VPN_IP" ]
		then
			SRCC="from"
			SRCA="$VPN_IP"
		else
			SRCC=""
			SRCA=""
		fi
		DST_IP=$(echo $ENTRY | cut -d ">" -f 3)
		if [ "$DST_IP" != "0.0.0.0" ] && [ -n "$DST_IP" ]
		then
			DSTC="to"
			DSTA="$DST_IP"
		else
			DSTC=""
			DSTA=""
		fi
		if [ -n "$SRCC" ] || [ -n "$DSTC" ]
		then
			ip rule add $SRCC $SRCA $DSTC $DSTA table $TARGET_LOOKUP priority $RULE_PRIO
			my_logger "Adding route for $VPN_IP to $DST_IP through $TARGET_NAME"
		fi
	done
	IFS=$OLDIFS
}

purge_client_list(){
	IP_LIST=$(ip rule show | cut -d ":" -f 1)
	for PRIO in $IP_LIST
	do
		if [ "$PRIO" -ge "$START_PRIO" ] && [ "$PRIO" -le "$END_PRIO" ]
		then
			ip rule del prio $PRIO
			my_logger "Removing rule $PRIO from routing policy"
		fi
	done
}

run_custom_script(){
	if [ -f /jffs/scripts/openvpn-event ]
	then
		/usr/bin/logger -t "custom_script" "Running /jffs/scripts/openvpn-event (args: $PARAM)"
		/bin/sh /jffs/scripts/openvpn-event $PARAM
	fi
}

init_table(){
	my_logger "Creating VPN routing table (mode $VPN_REDIR)"
	ip route flush table $VPN_TBL

# Fill it with copy of existing main table
	if [ "$VPN_REDIR" -eq 3 ]
	then
		LANIFNAME=$(nvram get lan_ifname)
		ip route show table main dev $LANIFNAME | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE dev $LANIFNAME
		done
		ip route show table main dev $dev | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE dev $dev
		done
	elif [ "$VPN_REDIR" -eq 2 ]
	then
		ip route show table main | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE
		done
	fi
}

Set_VPN_NVRAM_Vars() {

  	VPN_UNIT=$(echo "$dev" | awk '{ string=substr($0, 5, 5); print string; }')
  	VPN_IP_LIST="$(nvram get vpn_client"$VPN_UNIT"_clientlist)"
  	for n in 1 2 3 4 5; do
    		VPN_IP_LIST="${VPN_IP_LIST}$(nvram get vpn_client"$VPN_UNIT"_clientlist$n)"
  	done
  	VPN_REDIR=$(nvram get vpn_client"$VPN_UNIT"_rgw)
  	VPN_FORCE=$(nvram get vpn_client"$VPN_UNIT"_enforce)
  	VPN_LOGGING=$(nvram get vpn_client"$VPN_UNIT"_verb)
  	VPN_TBL="ovpnc${VPN_UNIT}"
  	START_PRIO=$((10000 + (200 * (VPN_UNIT - 1))))
  	END_PRIO=$((START_PRIO + 199))
  	WAN_PRIO=$START_PRIO
  	VPN_PRIO=$((START_PRIO + 100))
}

# Begin
logger -st "($(basename "$0"))" $$ "Starting routing policy configuration for client $VPN_UNIT"

case "$dev" in
tun11 | tun12 | tun13 | tun14 | tun15) Set_VPN_NVRAM_Vars ;;
*) run_custom_script && exit 0 ;;
esac

# webui reports that vpn_force changed while vpn client was down
if [ "$script_type" = "rmupdate" ]
then
	my_logger "Refreshing policy rules for client $VPN_UNIT"
	purge_client_list

	if [ "$VPN_FORCE" -eq 1 ] && [ "$VPN_REDIR" -ge 2 ]
	then
		init_table
		my_logger "Tunnel down - VPN client access blocked"
		ip route del default table $VPN_TBL
		ip route add prohibit default table $VPN_TBL
		create_client_list
	else
		my_logger "Allow WAN access to all VPN clients"
		ip route flush table $VPN_TBL
	fi
	ip route flush cache
	exit 0
fi

if [ "$script_type" = "route-up" ] && [ "$VPN_REDIR" -lt 2 ]
then
	my_logger "Skipping, client $VPN_UNIT not in routing policy mode"
	run_custom_script
	exit 0
fi

/usr/bin/logger -t "openvpn-routing" "Configuring policy rules for client $VPN_UNIT"

if [ "$script_type" = "route-pre-down" ]
then
	purge_client_list

	if [ "$VPN_FORCE" -eq 1 ] && [ "$VPN_REDIR" -ge 2 ]
	then
		/usr/bin/logger -t "openvpn-routing" "Tunnel down - VPN client access blocked"
		ip route change prohibit default table $VPN_TBL
		create_client_list
	else
		ip route flush table $VPN_TBL
		my_logger "Flushing client routing table"
	fi
fi	# End route down



if [ "$script_type" = "route-up" ]
then
	init_table

# Delete existing VPN routes that were pushed by server on table main
	NET_LIST=$(ip route show|awk '$2=="via" && $3==ENVIRON["route_vpn_gateway"] && $4=="dev" && $5==ENVIRON["dev"] {print $1}')
	for NET in $NET_LIST
	do
		ip route del $NET dev $dev
		my_logger "Removing route for $NET to $dev from main routing table"
	done

# Update policy rules
        purge_client_list
        create_client_list

# Setup table default route
	if [ -n "$VPN_IP_LIST" ]
	then
		if [ "$VPN_FORCE" -eq 1 ]
		then
			/usr/bin/logger -t "openvpn-routing" "Tunnel re-established, restoring WAN access to clients"
		fi
		if [ -n "$route_net_gateway" ]
		then
			ip route del default table $VPN_TBL
			ip route add default via $route_vpn_gateway table $VPN_TBL
		else
			/usr/bin/logger -t "openvpn-routing" "WARNING: no VPN gateway provided, routing might not work properly!"
		fi
	fi

	if [ -n "$route_net_gateway" ]
	then
		ip route del default
		ip route add default via $route_net_gateway
	fi
fi	# End route-up

ip route flush cache
my_logger "Completed routing policy configuration for client $VPN_UNIT"
run_custom_script

exit 0

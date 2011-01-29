/*
 * pptp.c
 *
 * Copyright (C) 2007 Sebastian Gottschall <gottschall@dd-wrt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 */
#ifdef HAVE_PPTPD
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <services.h>

void start_pptpd(void)
{
	int ret = 0, mss = 0;
	char *lpTemp;
	FILE *fp;

	if (!nvram_invmatch("pptpd_enable", "0")) {
		stop_pptpd();
		return;
	}
	// cprintf("stop vpn modules\n");
	// stop_vpn_modules ();

	// Create directory for use by pptpd daemon and its supporting files
	mkdir("/tmp/pptpd", 0744);
	cprintf("open options file\n");
	// Create options file that will be unique to pptpd to avoid interference 
	// with pppoe and pptp
	fp = fopen("/tmp/pptpd/options.pptpd", "w");
	cprintf("adding radius plugin\n");
	if (nvram_match("pptpd_radius", "1"))
		fprintf(fp, "plugin radius.so\nplugin radattr.so\n"
			"radius-config-file /tmp/pptpd/radius/radiusclient.conf\n");
	cprintf("check if wan_wins = zero\n");
	int nowins = 0;

	if (nvram_match("wan_wins", "0.0.0.0")) {
		nvram_set("wan_wins", "");
		nowins = 1;
	}
	if (strlen(nvram_safe_get("wan_wins")) == 0)
		nowins = 1;

	cprintf("write config\n");
	fprintf(fp, "lock\n"
		"name *\n"
		"nobsdcomp\n"
		"nodeflate\n"
		"auth\n"
		"refuse-pap\n"
		"refuse-eap\n"
		"refuse-chap\n" "refuse-mschap\n" "require-mschap-v2\n");
	if (nvram_match("pptpd_forcemppe", "1"))
		fprintf(fp, "mppe required,stateless\n");
	else
		fprintf(fp, "mppe stateless\n");
	fprintf(fp, "mppc\n" "debug\n" "logfd 2\n"
//disable 4 now         "connections 254\n"     //allows X concurrent connections (default 100)
		"ms-ignore-domain\n"
		"chap-secrets /tmp/pptpd/chap-secrets\n"
		"ip-up-script /tmp/pptpd/ip-up\n"
		"ip-down-script /tmp/pptpd/ip-down\n"
		"proxyarp\n"
		"ipcp-accept-local\n"
		"ipcp-accept-remote\n"
		"lcp-echo-failure 10\n"
		"lcp-echo-interval 6\n"
		"mtu %s\n" "mru %s\n",
		nvram_get("pptpd_mtu") ? nvram_get("pptpd_mtu") : "1450",
		nvram_get("pptpd_mru") ? nvram_get("pptpd_mru") : "1450");
	if (!nowins) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("wan_wins"));
	}
	if (strlen(nvram_safe_get("pptpd_wins1"))) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("pptpd_wins1"));
	}
	if (strlen(nvram_safe_get("pptpd_wins2"))) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("pptpd_wins2"));
	}

	struct dns_lists *dns_list = get_dns_list();

	if (nvram_match("dnsmasq_enable", "1")) {
		if (nvram_invmatch("lan_ipaddr", ""))
			fprintf(fp, "ms-dns %s\n",
				nvram_safe_get("lan_ipaddr"));
	} else if (nvram_match("local_dns", "1")) {
		if (dns_list && (nvram_invmatch("lan_ipaddr", "")
				 || strlen(dns_list->dns_server[0]) > 0
				 || strlen(dns_list->dns_server[1]) > 0
				 || strlen(dns_list->dns_server[2]) > 0)) {

			if (nvram_invmatch("lan_ipaddr", ""))
				fprintf(fp, "ms-dns %s\n",
					nvram_safe_get("lan_ipaddr"));
			if (strlen(dns_list->dns_server[0]) > 0)
				fprintf(fp, "ms-dns %s\n",
					dns_list->dns_server[0]);
			if (strlen(dns_list->dns_server[1]) > 0)
				fprintf(fp, "ms-dns %s\n",
					dns_list->dns_server[1]);
			if (strlen(dns_list->dns_server[2]) > 0)
				fprintf(fp, "ms-dns %s\n",
					dns_list->dns_server[2]);
		}
	} else {
		if (dns_list
		    && (strlen(dns_list->dns_server[0]) > 0
			|| strlen(dns_list->dns_server[1]) > 0
			|| strlen(dns_list->dns_server[2]) > 0)) {
			if (strlen(dns_list->dns_server[0]) > 0)
				fprintf(fp, "ms-dns  %s\n",
					dns_list->dns_server[0]);
			if (strlen(dns_list->dns_server[1]) > 0)
				fprintf(fp, "ms-dns  %s\n",
					dns_list->dns_server[1]);
			if (strlen(dns_list->dns_server[2]) > 0)
				fprintf(fp, "ms-dns  %s\n",
					dns_list->dns_server[2]);
		}
	}
	if (dns_list)
		free(dns_list);
	if (strlen(nvram_safe_get("pptpd_dns1"))) {
		fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns1"));
	}
	if (strlen(nvram_safe_get("pptpd_dns2"))) {
		fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns2"));
	}
	// Following is all crude and need to be revisited once testing confirms
	// that it does work
	// Should be enough for testing..
	if (nvram_match("pptpd_radius", "1")) {
		if (nvram_get("pptpd_radserver") != NULL
		    && nvram_get("pptpd_radpass") != NULL) {

			fclose(fp);

			mkdir("/tmp/pptpd/radius", 0744);

			fp = fopen("/tmp/pptpd/radius/radiusclient.conf", "w");
			fprintf(fp, "auth_order radius\n"
				"login_tries 4\n"
				"login_timeout 60\n"
				"radius_timeout 10\n"
				"nologin /etc/nologin\n"
				"servers /tmp/pptpd/radius/servers\n"
				"dictionary /etc/dictionary\n"
				"seqfile /var/run/radius.seq\n"
				"mapfile /etc/port-id-map\n"
				"radius_retries 3\n"
				"authserver %s:%s\n",
				nvram_get("pptpd_radserver"),
				nvram_get("pptpd_radport") ?
				nvram_get("pptpd_radport") : "radius");

			if (nvram_get("pptpd_radserver") != NULL
			    && nvram_get("pptpd_acctport") != NULL)
				fprintf(fp, "acctserver %s:%s\n",
					nvram_get("pptpd_radserver"),
					nvram_get("pptpd_acctport") ?
					nvram_get("pptpd_acctport") :
					"radacct");
			fclose(fp);

			fp = fopen("/tmp/pptpd/radius/servers", "w");
			fprintf(fp, "%s\t%s\n", nvram_get("pptpd_radserver"),
				nvram_get("pptpd_radpass"));
			fclose(fp);

		} else
			fclose(fp);
	} else
		fclose(fp);

	// Create pptpd.conf options file for pptpd daemon
	fp = fopen("/tmp/pptpd/pptpd.conf", "w");
	if (nvram_match("pptpd_bcrelay", "1"))
		fprintf(fp, "bcrelay %s\n", nvram_safe_get("lan_ifname"));
	fprintf(fp, "localip %s\n"
		"remoteip %s\n", nvram_safe_get("pptpd_lip"),
		nvram_safe_get("pptpd_rip"));
	fclose(fp);

	// Create ip-up and ip-down scripts that are unique to pptpd to avoid
	// interference with pppoe and pptp
	/*
	 * adjust for tunneling overhead (mtu - 40 byte IP - 108 byte tunnel
	 * overhead) 
	 */
	if (nvram_match("mtu_enable", "1"))
		mss = atoi(nvram_safe_get("wan_mtu")) - 40 - 108;
	else
		mss = 1500 - 40 - 108;
	char bcast[32];

	strcpy(bcast, nvram_safe_get("lan_ipaddr"));
	get_broadcast(bcast, nvram_safe_get("lan_netmask"));

	fp = fopen("/tmp/pptpd/ip-up", "w");
	fprintf(fp, "#!/bin/sh\n" "startservice set_routes\n"	// reinitialize 
		"echo $PPPD_PID $1 $5 $6 $PEERNAME >> /tmp/pptp_connected\n"	//
		"iptables -I FORWARD -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n"	//
		"iptables -I INPUT -i $1 -j ACCEPT\n" "iptables -I FORWARD -i $1 -j ACCEPT\n"	//
		"iptables -t nat -I PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s "	// rule for wake on lan over pptp tunnel
		"IN=`cat /var/run/radattr.$1 | grep -i RP-Upstream-Speed-Limit | awk '{print $2}'`\n"		//
		"OUT=`cat /var/run/radattr.$1 | grep -i RP-Downstream-Speed-Limit | awk '{print $2}'`\n"	//
		"if [ !-z $IN ] && [ $IN -gt 0 ] && [ $OUT -gt 0 ]"
		"then	tc qdisc del root dev $1\n"	//
		"	tc qdisc del dev $1 ingress\n"	//
		" 	tc qdisc add dev $1 root tbf rate \"$OUT\"kbit latency 50ms burst \"$OUT\"kbit\n"	//
		" 	tc qdisc add dev $1 handle ffff: ingress\n"	//
		" 	tc filter add dev $1 parent ffff: protocol ip prio 50 u32 match ip src 0.0.0.0/0 police rate \"$IN\"kbit burst \"$IN\"kbit drop flowid :1\n"
		"fi"
		"%s\n", bcast,
		nvram_get("pptpd_ipup_script") ?
		nvram_get("pptpd_ipup_script") : "");
	fclose(fp);
	fp = fopen("/tmp/pptpd/ip-down", "w");
	fprintf(fp, "#!/bin/sh\n" "grep -v $1  /tmp/pptp_connected > /tmp/pptp_connected.new\n"	//
		"mv /tmp/pptp_connected.new /tmp/pptp_connected\n"	//
		"iptables -D FORWARD -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n"	//
		"iptables -D INPUT -i $1 -j ACCEPT\n" "iptables -D FORWARD -i $1 -j ACCEPT\n"	//
		"iptables -t nat -D PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s "	// rule for wake on lan over pptp tunnel
		"tc qdisc del root dev $1\n"	//
		"tc qdisc del ingress dev $1\n"	//
		"%s\n", bcast,
		nvram_get("pptpd_ipdown_script") ?
		nvram_get("pptpd_ipdown_script") : "");
	fclose(fp);
	chmod("/tmp/pptpd/ip-up", 0744);
	chmod("/tmp/pptpd/ip-down", 0744);

	// Exctract chap-secrets from nvram and add the default account with
	// routers password
	lpTemp = nvram_safe_get("pptpd_auth");
	fp = fopen("/tmp/pptpd/chap-secrets", "w");
	// fprintf (fp, "root\t*\t%s\t*\n", nvram_safe_get ("http_passwd"));
	if (strlen(lpTemp) != 0)
		fprintf(fp, "%s\n", lpTemp);
	fclose(fp);

	chmod("/tmp/pptpd/chap-secrets", 0600);

	// Execute pptpd daemon
	ret =
	    eval("pptpd", "-c", "/tmp/pptpd/pptpd.conf", "-o",
		 "/tmp/pptpd/options.pptpd");

	dd_syslog(LOG_INFO, "pptpd : pptp daemon successfully started\n");
	return;
}

void stop_pptpd(void)
{

	stop_process("pptpd", "pptp server");
	stop_process("bcrelay", "pptp broadcast relay");
	return;
}
#endif

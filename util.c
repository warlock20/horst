/* horst - olsr scanning tool
 *
 * Copyright (C) 2005-2007  Bruno Randolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include "util.h"
#include "ieee80211.h"


/* a list of packet type names for easier indexing with padding */
struct pkt_names mgmt_names[] = {
	{ 'a', "ASSOC_REQ" },		/* IEEE80211_STYPE_ASSOC_REQ	0x0000 */
	{ 'A', "ASSOC_RESP" },		/* IEEE80211_STYPE_ASSOC_RESP	0x0010 */
	{ 'a', "REASSOC_REQ" },		/* IEEE80211_STYPE_REASSOC_REQ	0x0020 */
	{ 'A', "REASSOC_RESP" },	/* IEEE80211_STYPE_REASSOC_RESP	0x0030 */
	{ 'p', "PROBE_REQ" },		/* IEEE80211_STYPE_PROBE_REQ	0x0040 */
	{ 'P', "PROBE_RESP" },		/* IEEE80211_STYPE_PROBE_RESP	0x0050 */
	{}, {}, 			/* unused */
	{ 'B', "BEACON" },		/* IEEE80211_STYPE_BEACON	0x0080 */
	{ 't', "ATIM" },		/* IEEE80211_STYPE_ATIM		0x0090 */
	{ 'D', "DISASSOC" },		/* IEEE80211_STYPE_DISASSOC	0x00A0 */
	{ 'u', "AUTH" },		/* IEEE80211_STYPE_AUTH		0x00B0 */
	{ 'U', "DEAUTH" },		/* IEEE80211_STYPE_DEAUTH	0x00C0 */
	{ 'T', "ACTION" },		/* IEEE80211_STYPE_ACTION	0x00D0 */
};

struct pkt_names ctl_names[] = {
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, /* let's waste some memory ;) */
	{ 's', "PSPOLL" },		/* IEEE80211_STYPE_PSPOLL	0x00A0 */
	{ 'R', "RTS" },			/* IEEE80211_STYPE_RTS		0x00B0 */
	{ 'C', "CTS" },			/* IEEE80211_STYPE_CTS		0x00C0 */
	{ 'K', "ACK" },			/* IEEE80211_STYPE_ACK		0x00D0 */
	{ 'f', "CFEND" },		/* IEEE80211_STYPE_CFEND	0x00E0 */
	{ 'f', "CFENDACK" },		/* IEEE80211_STYPE_CFENDACK	0x00F0 */
};

struct pkt_names data_names[] = {
	{ 'D', "DATA" },		/* IEEE80211_STYPE_DATA			0x0000 */
	{ 'F', "DATA_CFACK" },		/* IEEE80211_STYPE_DATA_CFACK		0x0010 */
	{ 'F', "DATA_CFPOLL" },		/* IEEE80211_STYPE_DATA_CFPOLL		0x0020 */
	{ 'F', "DATA_CFACKPOLL" },	/* IEEE80211_STYPE_DATA_CFACKPOLL	0x0030 */
	{ 'n', "NULLFUNC" },		/* IEEE80211_STYPE_NULLFUNC		0x0040 */
	{ 'f', "CFACK" },		/* IEEE80211_STYPE_CFACK		0x0050 */
	{ 'f', "CFPOLL" },		/* IEEE80211_STYPE_CFPOLL		0x0060 */
	{ 'f', "CFACKPOLL" },		/* IEEE80211_STYPE_CFACKPOLL		0x0070 */
	{ 'Q', "QOS_DATA" },		/* IEEE80211_STYPE_QOS_DATA		0x0080 */
	{ 'F', "QOS_DATA_CFACK" },	/* IEEE80211_STYPE_QOS_DATA_CFACK	0x0090 */
	{ 'F', "QOS_DATA_CFPOLL" },	/* IEEE80211_STYPE_QOS_DATA_CFPOLL	0x00A0 */
	{ 'F', "QOS_DATA_CFACKPOLL" },	/* IEEE80211_STYPE_QOS_DATA_CFACKPOLL	0x00B0 */
	{ 'N', "QOS_DATA_NULLFUNC" },	/* IEEE80211_STYPE_QOS_NULLFUNC		0x00C0 */
	{ 'f', "QOS_CFACK" },		/* IEEE80211_STYPE_QOS_CFACK		0x00D0 */
	{ 'f', "QOS_CFPOLL" },		/* IEEE80211_STYPE_QOS_CFPOLL		0x00E0 */
	{ 'f', "QOS_CFACKPOLL" },	/* IEEE80211_STYPE_QOS_CFACKPOLL	0x00F0 */
};


inline int
normalize(int oval, float max_val, int max) {
	int val;
	val=(oval/max_val)*max;
	if (val>max)
		val=max; /* cap if still bigger */
	if (val==0 && oval > 0)
		val=1;
	if (val<0)
		val=0;
	return val;
}


void
dump_packet(const unsigned char* buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if ((i%2) == 0)
			DEBUG(" ");
		if ((i%16) == 0)
			DEBUG("\n");
		DEBUG("%02x", buf[i]);
	}
	DEBUG("\n");
}


const char*
ether_sprintf(const unsigned char *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}


const char*
ip_sprintf(const unsigned int ip)
{
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}


void
convert_string_to_mac(const char* string, unsigned char* mac)
{
	int c;
	for(c=0; c < 6 && string; c++)
	{
		int x = 0;
		if (string)
			sscanf(string, "%x", &x);
		mac[c] = x;
		string = strchr(string, ':');
		if (string)
			string++;
	}
}


char
get_paket_type_char(int type)
{
	switch (type & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_MGMT:
		return MGMT_NAME(type & IEEE80211_FCTL_STYPE).c;

	case IEEE80211_FTYPE_CTL:
		return CTL_NAME(type & IEEE80211_FCTL_STYPE).c;

	case IEEE80211_FTYPE_DATA:
		return DATA_NAME(type & IEEE80211_FCTL_STYPE).c;
	}
	return '?';
}


char*
get_paket_type_name(int type)
{
	switch (type & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_MGMT:
		return MGMT_NAME(type & IEEE80211_FCTL_STYPE).name;

	case IEEE80211_FTYPE_CTL:
		return CTL_NAME(type & IEEE80211_FCTL_STYPE).name;

	case IEEE80211_FTYPE_DATA:
		return DATA_NAME(type & IEEE80211_FCTL_STYPE).name;
	}
	return "UNK";
}
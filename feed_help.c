/* NeoStats - IRC Statistical Services 
** Copyright (c) 1999-2006 Adam Rutter, Justin Hammond, Mark Hetherington
** http://www.neostats.net/
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
**  USA
**
** NeoStats CVS Identification
** $Id: feed_help.c 210 2006-01-26 15:35:09Z Fish $
*/

#include "neostats.h"

const char *feed_about[] = {
	"\2Open Proxy Scanning Bot Information\2",
	"",
	"This service scans clients connecting to this network for",
	"insecure proxies. Insecure proxies are often used to attack",
	"networks or channels with clone bots. If you have a firewall,",
	"or IDS software, please ignore any errors that this scan",
	"may generate.",
	"",
	"If you have any further questions, please contact network",
	"administration.",
	NULL
};

const char *feed_help_subscribe[] = {
	"\2Open Proxy Scanning Bot Information\2",
	"",
	"This service scans clients connecting to this network for",
	"insecure proxies. Insecure proxies are often used to attack",
	"networks or channels with clone bots. If you have a firewall,",
	"or IDS software, please ignore any errors that this scan",
	"may generate.",
	"",
	"If you have any further questions, please contact network",
	"administration.",
	NULL
};

const char *feed_help_add[] = {
	"Add a port to scanning",
	"Syntax: \2ADD <type> <port>\2",
	"",
	"Add an entry to the port scan list.",
	"<type> must be one of:", 
	"    HTTP, HTTPPOST, SOCKS4, SOCKS5, WINGATE, ROUTER",
	"<port> must be a valid port number.",
	"The new port is scanned straight away",
	NULL
};
const char *feed_help_info[] = {
	"Add a port to scanning",
	"Syntax: \2ADD <type> <port>\2",
	"",
	"Add an entry to the port scan list.",
	"<type> must be one of:", 
	"    HTTP, HTTPPOST, SOCKS4, SOCKS5, WINGATE, ROUTER",
	"<port> must be a valid port number.",
	"The new port is scanned straight away",
	NULL
};

const char *feed_help_del[] = {
	"Delete a port from scanning",
	"Syntax: \2DEL <index>\2",
	"",
	"Delete entry <index> from the list of ports. ",
	"Requires a restart of OPSB to become effective.",
	NULL
};

const char *feed_help_list[] = {
	"List protocols and ports scanned",
	"Syntax: \2LIST\2",
	"",
	"List the current ports and protocols scanned",
	"and a ID number for use in removing entries.",
	NULL
};

const char *feed_help_set_exclusions[] = {
	"\2EXCLUSIONS <ON|OFF>\2",
	"Use global exclusion list in addition to local exclusion list",
	NULL
};

const char *feed_help_set_verbose [] = {
	"\2VERBOSE <ON|OFF>\2",
	"Whether OPSB is verbose in operation or not",
	NULL
};

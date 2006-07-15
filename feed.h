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
** NetStats CVS Identification
** $Id: feed.h 210 2006-01-26 15:35:09Z Fish $
*/

#ifndef FEED_H
#define FEED_H

#include MODULECONFIG

extern Bot *feed_bot;

typedef struct feedcfg {
  int verbose;
  int  exclusions;
} feedcfg;

extern feedcfg feed;
 
/* help text */
extern const char *feed_about[];
extern const char *feed_help_add[];
extern const char *feed_help_del[];
extern const char *feed_help_info[];
extern const char *feed_help_list[];

extern const char *feed_help_set_exclusions[];
extern const char *feed_help_set_verbose[];
#endif /* FEED_H */

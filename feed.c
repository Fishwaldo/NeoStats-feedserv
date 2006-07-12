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
** $Id: feed.c 210 2006-01-26 15:35:09Z Fish $
*/

#include "neostats.h"
#include "feed.h"
#include "mrss.h"

static int feed_cmd_list( const CmdParams *cmdparams );
static int feed_cmd_add( const CmdParams *cmdparams );
static int feed_cmd_del( const CmdParams *cmdparams );
static int feed_set_exclusions_cb( const CmdParams *cmdparams, SET_REASON reason );
static void FeedDownLoadHandler(void *ptr, int status, char *data, int datasize);
Bot *feed_bot;
typedef struct feeddata {
	mrss_t *mrss;
} feeddata;


/** Copyright info */
static const char *feed_copyright[] = {
	"Copyright (c) 1999-2006, NeoStats",
	"http://www.neostats.net/",
	NULL
};

/** Module Info definition 
 * version information about our module
 * This structure is required for your module to load and run on NeoStats
 */
ModuleInfo module_info = {
	"FeedServ",
	"a RSS Scrapping Bot",
	feed_copyright,
	feed_about,
	NEOSTATS_VERSION,
	MODULE_VERSION,
	__DATE__,
	__TIME__,
	MODULE_FLAG_LOCAL_EXCLUDES,
	0,
	0,
};

static bot_cmd feed_commands[]=
{
	{"ADD",		feed_cmd_add,		2,	NS_ULEVEL_ADMIN,	feed_help_add, 0, NULL, NULL},
	{"DEL",		feed_cmd_del,		1,	NS_ULEVEL_ADMIN,	feed_help_del, 0, NULL, NULL},
	{"LIST",	feed_cmd_list,		0,	NS_ULEVEL_ADMIN,	feed_help_list, 0, NULL, NULL},
	NS_CMD_END()
};

static bot_setting feed_settings[]=
{
	{"VERBOSE",		&feed.verbose,		SET_TYPE_BOOLEAN,	0,	0,	NS_ULEVEL_ADMIN, 	NULL,	feed_help_set_verbose,	NULL, (void*)1 	},
	{"EXCLUSIONS",	&feed.exclusions,	SET_TYPE_BOOLEAN,	0,	0,	NS_ULEVEL_ADMIN,	NULL,	feed_help_set_exclusions,	feed_set_exclusions_cb, (void *)0 },
	NS_SETTING_END()
};

/** BotInfo */
static BotInfo feed_botinfo = 
{
	"FeedServ", 
	"RSSServ", 
	"news", 
	BOT_COMMON_HOST, 
	"RSS Scrapping/news Bot", 	
	BOT_FLAG_SERVICEBOT|BOT_FLAG_RESTRICT_OPERS|BOT_FLAG_DEAF, 
	feed_commands, 
	feed_settings,
};

ModuleEvent module_events[] = 
{
	NS_EVENT_END()
};



/** @brief feed_cmd_list
 *
 *  LIST command handler
 *
 *  @param cmdparam struct
 *
 *  @return NS_SUCCESS if suceeds else result of command
 */

int feed_cmd_list (const CmdParams *cmdparams) 
{

	SET_SEGV_LOCATION();
	return NS_SUCCESS;
}

/** @brief feed_cmd_add
 *
 *  ADD command handler
 *
 *  @param cmdparam struct
 *
 *  @return NS_SUCCESS if suceeds else result of command
 */

int feed_cmd_add (const CmdParams *cmdparams) 
{

	SET_SEGV_LOCATION();
	return NS_SUCCESS;
}

/** @brief feed_cmd_del
 *
 *  DEL command handler
 *
 *  @param cmdparam struct
 *
 *  @return NS_SUCCESS if suceeds else result of command
 */

int feed_cmd_del (const CmdParams *cmdparams) 
{
	SET_SEGV_LOCATION();
	return NS_SUCCESS;
}

/** @brief feed_set_exclusions_cb
 *
 *  Set callback for exclusions
 *  Enable or disable exclude event flag
 *
 *  @cmdparams pointer to commands param struct
 *  @cmdparams reason for SET
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

static int feed_set_exclusions_cb( const CmdParams *cmdparams, SET_REASON reason )
{
	SET_SEGV_LOCATION();

	if( reason == SET_LOAD || reason == SET_CHANGE )
	{
		SetAllEventFlags( EVENT_FLAG_USE_EXCLUDE, feed.exclusions );
	}
	return NS_SUCCESS;
}

static void ScheduleFeeds() {
	feeddata *ptr;
	SET_SEGV_LOCATION();
	ptr = ns_malloc(sizeof(feeddata));
	ptr->mrss = NULL;
	mrss_new(&ptr->mrss);
	if (new_transfer("http://slashdot.org/index.rss", NULL, NS_MEMORY, "", ptr, FeedDownLoadHandler) != NS_SUCCESS ) {
		nlog(LOG_WARNING, "Download Feed Failed");
		irc_chanalert(feed_bot, "Download Feed Failed");
		mrss_free(ptr->mrss);
		ns_free(ptr);
		return;
	}
}

/** @brief CheckFeed
*/
static void CheckFeed(feeddata *ptr) {

}

/** @brief FeedDownLoadHandler
*/
static void FeedDownLoadHandler(void *usrptr, int status, char *data, int datasize) {
	mrss_error_t ret;
	feeddata *ptr = (feeddata *)usrptr;
	SET_SEGV_LOCATION();
	if (status != NS_SUCCESS) {
		dlog(DEBUG1, "RSS Scrape Download failed: %s", data);
		irc_chanalert(feed_bot, "RSS Scrape Failed: %s", data);
		mrss_free((feeddata *)ptr->mrss);
		ns_free(ptr);
		return;
	}
	/* ok, here is our data */
	ret = mrss_parse_buffer(data, datasize, &ptr->mrss);
	if (ret != MRSS_OK) {
		dlog(DEBUG1, "RSS Parse Failed: %s", mrss_strerror(ret));
		mrss_free((feeddata *)ptr->mrss);
		ns_free(ptr);
		return;
	}
	CheckFeed((feeddata *)ptr);
	return;	
}

/** @brief ModInit
 *
 *  Init handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModInit( void )
{

	SET_SEGV_LOCATION();

	return NS_SUCCESS;
}

/** @brief ModSynch
 *
 *  Startup handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModSynch (void)
{
	SET_SEGV_LOCATION();
	feed_bot = AddBot (&feed_botinfo);
	ScheduleFeeds();
	return NS_SUCCESS;
}

/** @brief ModFini
 *
 *  Fini handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModFini( void )
{
	return NS_SUCCESS;
}


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
static int feed_cmd_info( const CmdParams *cmdparams );
static int feed_set_exclusions_cb( const CmdParams *cmdparams, SET_REASON reason );
static int feed_cmd_subscribe(const CmdParams *cmdparams);
static int fs_subscribe_list(Client *c, char *what);
static int fs_signoff_user(const CmdParams *cmdparams);
static void FeedDownLoadHandler(void *ptr, int status, char *data, int datasize);
static void FeedRequestHandler(void *usrptr, int status, char *data, int datasize);

char *trim (char *tmp);

Bot *feed_bot;
typedef struct feeddata {
	mrss_t *mrss;
	char *channel;
} feeddata;

feedcfg feed;

typedef struct feedinfo {
	char title[BUFSIZE];
	char description[BUFSIZE];
	char link[BUFSIZE];
	char about[BUFSIZE];
	char feedurl[BUFSIZE];
	int setup;
} feedinfo;

typedef struct feedcache {
	char *item;
	char *link;
	char *description;
	char *author;
	char *comments;
	char *guid;
	char *date;
	char *hash;
} feedcache;

typedef struct subscribed {
	hash_t *users;
	hash_t *chans;
	feedinfo *feed;
	list_t *cache;
	int active;
	int lastcheck;
} subscribed;

typedef struct feedrequest {
	Client *c;
	feedinfo *fi;
} feedrequest;

list_t *lofeeds;
hash_t *subscribedfeeds;

static int fs_subscribe(Client *c, feedinfo *fi);


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
	{"ADD",		feed_cmd_add,		1,	NS_ULEVEL_ADMIN,	feed_help_add, 0, NULL, NULL},
	{"DEL",		feed_cmd_del,		1,	NS_ULEVEL_ADMIN,	feed_help_del, 0, NULL, NULL},
	{"LIST",	feed_cmd_list,		0,	NS_ULEVEL_ADMIN,	feed_help_list, 0, NULL, NULL},
	{"INFO", 	feed_cmd_info,		1,	NS_ULEVEL_ADMIN,	feed_help_info, 0, NULL, NULL},
	{"SUBSCRIBE", 	feed_cmd_subscribe,	1,	NS_ULEVEL_ADMIN,	feed_help_subscribe, 0, NULL, NULL},
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
	{ EVENT_QUIT, fs_signoff_user, 0},
	{ EVENT_KILL, fs_signoff_user, 0},
	NS_EVENT_END()
};

static int
html_trim_check (char *tmp)
{
  int i = 0;

  while (*tmp)
    {
      if (*tmp == '>')
	return i;
      tmp++;
      i++;
    }

  return 0;
}

char *
html_trim (char *tmp)
{
  int i, j, l, len;
  char *ret;

  tmp = trim (tmp);
  len = strlen (tmp);


  ret = malloc (sizeof (char) * (len + 1));

  for (i = j = 0; i < len; i++)
    {
      if (tmp[i] != '<' || !(l = html_trim_check (tmp + i)))
	ret[j++] = tmp[i];
      else
	i += l;
    }

  ret[j] = 0;
  free (tmp);


  return ret;
}

char *
trim (char *tmp)
{
  int i = 0;
  int len, j;
  char *ret;
  int q, ok = 0;

  while (tmp[i] == ' ' || tmp[i] == '\t' || tmp[i] == '\r' || tmp[i] == '\n')
    tmp++;

  i = strlen (tmp);
  i--;

  while (tmp[i] == ' ' || tmp[i] == '\t' || tmp[i] == '\r' || tmp[i] == '\n')
    i--;

  tmp[i + 1] = 0;

  len = strlen (tmp);

  if (len > 400)
    {
      ok = 1;
      len = 400;
    }

  ret = malloc (sizeof (char) * (len + 1 + (ok ? 3 : 0)));

  for (i = j = q = 0; i < len; i++)
    {
      if (tmp[i] == '\t' || tmp[i] == ' ' || tmp[i] == '\v' || tmp[i] == '\r'
	  || tmp[i] == '\n')
	{
	  if (!q)
	    {
	      q = 1;
	      ret[j++] = ' ';
	    }
	}
      else
	{
	  q = 0;
	  ret[j++] = tmp[i];
	}
    }

  if (ok)
    {
      ret[j++] = '.';
      ret[j++] = '.';
      ret[j++] = '.';
    }

  ret[j++] = 0;

  tmp = strdup (ret);
  free (ret);

  return tmp;
}


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
	feedinfo *fi;
	lnode_t *node;
	int i = 1;
	int doing = 0;
	SET_SEGV_LOCATION();
	node = list_first(lofeeds);
	irc_prefmsg(feed_bot, cmdparams->source, "Available Feed Listing:");
	while (node) {
		fi = lnode_get(node);
		if (fi->setup == 1) {
			irc_prefmsg(feed_bot, cmdparams->source, "%d) Title: %s", i, fi->title);
		} else {
			if ((cmdparams->ac > 0) && (!strcasecmp(cmdparams->av[0], "all"))) {
				irc_prefmsg(feed_bot, cmdparams->source, "%d) FeedURL: %s (Invalid)", i, fi->feedurl);
			}
			doing++;	
		}
		i++;
		node = list_next(lofeeds, node);
	}
	irc_prefmsg(feed_bot, cmdparams->source, "End of List.");
	if (doing > 0)  irc_prefmsg(feed_bot, cmdparams->source, "Still Setting up %d feeds", doing);
	CommandReport(feed_bot, "%s requested a feed listing", cmdparams->source->name);
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
	feedinfo *fi;
	feedrequest *fr;
	lnode_t *node;
	SET_SEGV_LOCATION();
	node = list_first(lofeeds);
	while (node) {
		fi = lnode_get(node);
		if (!strcasecmp(fi->feedurl, cmdparams->av[0])) {
			/* it exists, don't add */
			irc_prefmsg(feed_bot, cmdparams->source, "A feed with that URL already exists: %s", fi->title);
			return NS_SUCCESS;
		}
		node = list_next(lofeeds, node);
	}	
	/* if we are here, add it */
	fr = ns_malloc(sizeof(feedrequest));
	fr->c = cmdparams->source;
	fr->fi = ns_malloc(sizeof(feedinfo));
	strncpy(fr->fi->feedurl, cmdparams->av[0], BUFSIZE);
	if (new_transfer(fr->fi->feedurl, NULL, NS_MEMORY, "", fr, FeedRequestHandler) != NS_SUCCESS ) {
		irc_prefmsg(feed_bot, cmdparams->source, "Feed Setup Failed");
		ns_free(fr->fi);
		ns_free(fr);
		return NS_SUCCESS;
	}
	irc_prefmsg(feed_bot, cmdparams->source, "Checking Feed is valid. Please wait...");
	return NS_SUCCESS;
}


/** @brief feed_cmd_info
 *
 *  ADD command handler
 *
 *  @param cmdparam struct
 *
 *  @return NS_SUCCESS if suceeds else result of command
 */

int feed_cmd_info (const CmdParams *cmdparams) 
{
	feedinfo *fi;
	lnode_t *node;
	int i = 1;
	SET_SEGV_LOCATION();
	node = list_first(lofeeds);
	while (node) {
		if (i == atoi(cmdparams->av[0])) {
			fi = lnode_get(node);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed Title: %s", fi->title);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed URL: %s", fi->link);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed Description: %s", fi->description);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed RSS Link: %s", fi->feedurl);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed About: %s",fi->about);
			return NS_SUCCESS;
		}
		i++;
		node = list_next(lofeeds, node);
	}	
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
	feedinfo *fi;
	lnode_t *node;
	int i = 1;
	SET_SEGV_LOCATION();
	node = list_first(lofeeds);
	while (node) {
		if (i == atoi(cmdparams->av[0])) {
			fi = lnode_get(node);
			irc_prefmsg(feed_bot, cmdparams->source, "Feed %s has been deleted", fi->title);
			DBADelete("feeds", fi->feedurl);
			ns_free(fi);
			list_delete(lofeeds, node);
			return NS_SUCCESS;
		}
		i++;
		node = list_next(lofeeds, node);
	}	
	return NS_SUCCESS;
}

/** @brief feed_cmd_subscribe
 *
 *  subscribe to a feed, either by number, or by a pattern
 */
static int feed_cmd_subscribe(const CmdParams *cmdparams)
{
	feedinfo *fi;
	lnode_t *node;
	int i = 1;
	int get = 0;
	SET_SEGV_LOCATION();

	if (!strcasecmp("list", cmdparams->av[0])) {
		fs_subscribe_list(cmdparams->source, cmdparams->ac == 2 ? cmdparams->av[1] : NULL);
		return NS_SUCCESS;
	}

	get = atoi(cmdparams->av[0]);
	
	node = list_first(lofeeds);
	while (node) {
		fi = lnode_get(node);
		if ((get > 0) && (i == get)) {
			irc_prefmsg(feed_bot, cmdparams->source, "Subscribing to %s", fi->title);
			fs_subscribe(cmdparams->source, fi);
		} else if (match(cmdparams->av[0], fi->title)) {
			irc_prefmsg(feed_bot, cmdparams->source, "Subscribing to %s (Matched on Title)", fi->title);
			fs_subscribe(cmdparams->source, fi);
		} else if (match(cmdparams->av[0], fi->description)) {
			irc_prefmsg(feed_bot, cmdparams->source, "Subscribing to %s (Matched on Description)", fi->title);
			fs_subscribe(cmdparams->source, fi);
		}
		node = list_next(lofeeds, node);
		i++;
	}
	return NS_SUCCESS;
}

static int fs_check_subscriptions( void *unused) {
	hnode_t *hnode;
	hscan_t hscan;
	subscribed *sub;
	hash_scan_begin(&hscan, subscribedfeeds);
	while ((hnode = hash_scan_next(&hscan)) != NULL) {
		sub = hnode_get(hnode);
		if (sub->active != 1) 
			continue;
		if ((sub->lastcheck + feed.interval) > (int) me.now) 
			continue;
		/* if we are here, its active, and its ready for a scan */
		if (new_transfer(sub->feed->feedurl, NULL, NS_MEMORY, "", sub, FeedDownLoadHandler) != NS_SUCCESS ) {
			nlog(LOG_WARNING, "Download Feed Failed");
			irc_chanalert(feed_bot, "Download Feed Failed");
			continue;
		}
		sub->lastcheck = me.now;
	}
	return NS_SUCCESS;
}

static int fs_subscribe(Client *C, feedinfo *fi) {
	SET_SEGV_LOCATION();
	hnode_t *hnode;
	subscribed *sub;
	hash_t *userhash;
	hnode = hnode_find(subscribedfeeds, fi->feedurl);
	if (!hnode) {
		/* its a new subscription for feeds */
		sub = ns_malloc(sizeof(subscribed));
		sub->users = hash_create(HASHCOUNT_T_MAX, NULL, NULL);
		sub->chans = hash_create(HASHCOUNT_T_MAX, NULL, NULL);
		sub->cache = list_create(LISTCOUNT_T_MAX);
		sub->feed = fi;
		hnode_create_insert(subscribedfeeds, sub, sub->feed->feedurl);
	} else {
		sub = hnode_get(hnode);
	}
	if (sub->active != 1) {
		sub->lastcheck = me.now;
		sub->active = 1;
	}
	/* subscribe this user */
	hnode_create_insert(sub->users, C->name, C->name);
	/* store this feed in the users feed list */
	userhash = (hash_t *) GetUserModValue(C);
	if (!userhash) {
		userhash = hash_create(HASHCOUNT_T_MAX, NULL, NULL);
		SetUserModValue(C, userhash);
	}
	hnode_create_insert(userhash, sub, sub->feed->feedurl);
	DBAStore("feedsusers", sub->feed->feedurl, (void *)C->name, sizeof(C->name));
	if (!FindTimer("CheckSubscriptions")) {
		/* timer is not active */
		/* XXX update interval for production... 10 is for debug */
		AddTimer(TIMER_TYPE_INTERVAL, fs_check_subscriptions, "CheckSubscriptions", 10, NULL);
	}
	return NS_SUCCESS;
}

/** @brief fs_subscribe_list
 *
 *  display list of subscribed feeds
 *
 *  @cmdparams pointer to the client
 *  @cmdparams optional channel name
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

static int fs_subscribe_list(Client *c, char *what) {

}

/** @brief fs_signoff_user
 *
 *  signoff a user and delete any subscribed feeds he has
 *
 *  @cmdparams pointer to the client
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

static int fs_signoff_user(const CmdParams *cmdparams) {
	hash_t *userhash;
	hnode_t *hnode, *subfeednode, *usernode;

	hscan_t hscan;
	subscribed *sub;
	userhash = (hash_t *) GetUserModValue(cmdparams->source);
	if (!userhash) 
		return NS_SUCCESS;
	/* else, cycle through all the feeds and unsub them */
	hash_scan_begin(&hscan, userhash);
	while ((hnode = hash_scan_next(&hscan)) != NULL) {
		sub = hnode_get(hnode);
		/* find the feed in the subscribed feeds hash */
		subfeednode = hnode_find(subscribedfeeds, sub->feed->feedurl);
			if (!subfeednode) {
			 	nlog(LOG_WARNING, "Subscribers Feed Inconsistancies");
				continue;
			}
			/* if the count of users is 1 and chans is 0, then we can just remove the subscription */
			if ((hash_count(sub->users) == 1) && (hash_count(sub->chans) == 0)) {
printf("%s\n", sub->feed->feedurl);
printf("%s\n", hnode_getkey(subfeednode));
				hash_delete_destroy_node(subscribedfeeds, subfeednode);
			} else {
				/* the feed is more complex */
				usernode = hnode_find(sub->users, cmdparams->source->name);
				if (usernode) {
					hash_delete_destroy_node(sub->users, usernode);
				} else {
					nlog(LOG_WARNING, "Subscribers Users Hash is busted");
				}
			}								
	}
	hash_destroy(userhash);
	ClearUserModValue(cmdparams->source);
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

/** @brief SendToSubscribers
 */
static void SendToSubscribers(subscribed *subs, char *fmt, ...) {
	char buf[BUFSIZE];
	va_list va;
	char *target;
	hnode_t *hnode;
	hscan_t hscan;
	va_start(va, fmt);
	ircvsnprintf(buf, BUFSIZE, fmt, va);
	va_end(va);
	hash_scan_begin(&hscan, subs->users);
	while ((hnode = hash_scan_next(&hscan)) != NULL) {
		target = hnode_get(hnode);
		irc_prefmsg(feed_bot, FindClient(target), "%s", buf);
	}			
	hash_scan_begin(&hscan, subs->chans);
	while ((hnode = hash_scan_next(&hscan)) != NULL) {
		target = hnode_get(hnode);
		irc_chanprivmsg(feed_bot, target, "%s", buf);
	}			
}

/** @brief CheckFeed
*/
static void CheckFeed(subscribed *subs, mrss_t *mrss) {
  mrss_item_t *item;
  mrss_hour_t *hour;
  mrss_day_t *day;
  mrss_category_t *category;

  SendToSubscribers(subs, "New %s Stories:", subs->feed->title);
  item = mrss->item;
  while (item)
    {
      SendToSubscribers(subs ,"Title: %s", item->title);
      SendToSubscribers(subs ,"Text: %s", html_trim(item->description));
      SendToSubscribers(subs ,"Date: %s", item->pubDate);
      SendToSubscribers(subs ,"Link: %s", item->link);
      SendToSubscribers(subs ,"==============================================================");
    item = item->next;
    }
    
  SendToSubscribers(subs, "End of Stories from %s", subs->feed->title);
}

/** @brief FeedDownLoadHandler
*/
static void FeedDownLoadHandler(void *usrptr, int status, char *data, int datasize) {
	mrss_error_t ret;
	mrss_t *mrss;
	subscribed *ptr = (subscribed *)usrptr;
	SET_SEGV_LOCATION();
	if (status != NS_SUCCESS) {
		dlog(DEBUG1, "RSS Scrape Download failed: %s", data);
		irc_chanalert(feed_bot, "RSS Scrape Failed: %s", data);
		return;
	}
	/* ok, here is our data */
	mrss = NULL;
	mrss_new(&mrss);
	ret = mrss_parse_buffer(data, datasize, &mrss);
	if (ret != MRSS_OK) {
		dlog(DEBUG1, "RSS Parse Failed: %s", mrss_strerror(ret));
		mrss_free(mrss);
		return;
	}
	CheckFeed(ptr, mrss);
	mrss_free(mrss);
	return;	
}

static int load_feeds(char *key,  void *data, int size )
{
	feedinfo *fi;	
	if( size == sizeof(feedinfo) )
	{
		fi = ns_malloc( sizeof(feedinfo));
		os_memcpy(fi, data, sizeof (feedinfo));
		lnode_create_append(lofeeds, fi);
	}
	return NS_FALSE;
}

/** @brief FeedSetupHandler
*/
static void FeedSetupHandler(void *usrptr, int status, char *data, int datasize) {
	
	feedinfo *ptr = lnode_get((lnode_t *)usrptr);
	feedinfo *next = NULL;
	mrss_t *mrss;
	mrss_error_t ret;
	lnode_t *node;
	SET_SEGV_LOCATION();


	if (status != NS_SUCCESS) {
		dlog(DEBUG1, "RSS Scrape Download failed: %s", data);
		/* ok, move onto the next url */
		node = list_next(lofeeds, (lnode_t *)usrptr);
		if (node) next = lnode_get(node);
		if ((next) && ((new_transfer(next->feedurl, NULL, NS_MEMORY, "", node, FeedSetupHandler) != NS_SUCCESS ))) {
			nlog(LOG_WARNING, "Download Feed Setup failed");
		}
		return;
	}
	mrss = NULL;
	mrss_new(&mrss);
	/* ok, here is our data */
	ret = mrss_parse_buffer(data, datasize, &mrss);
	if (ret != MRSS_OK) {
		dlog(DEBUG1, "RSS Parse Failed: %s", mrss_strerror(ret));
		mrss_free(mrss);
		/* ok, move onto the next url */
		node = list_next(lofeeds, (lnode_t *)usrptr);
		if (node) next = lnode_get(node);
		if ((next) && ((new_transfer(next->feedurl, NULL, NS_MEMORY, "", node, FeedSetupHandler) != NS_SUCCESS ))) {
			nlog(LOG_WARNING, "Download Feed Setup failed");
		}
		return;
	}
	strncpy(ptr->title, mrss->title, BUFSIZE);
	if (mrss->description)
		strncpy(ptr->description, mrss->description, BUFSIZE);
	if (mrss->link)
		strncpy(ptr->link, mrss->link, BUFSIZE);
	if (mrss->about) 
		strncpy(ptr->about, mrss->about, BUFSIZE);
	ptr->setup = 1;
	DBAStore("feeds", ptr->feedurl, (void *)ptr, sizeof(feedinfo));

	mrss_free(mrss);


	/* ok, move onto the next url */
	node = list_next(lofeeds, (lnode_t *)usrptr);
	if (node) next = lnode_get(node);
	if ((next) && ((new_transfer(next->feedurl, NULL, NS_MEMORY, "", node, FeedSetupHandler) != NS_SUCCESS ))) {
			nlog(LOG_WARNING, "Download Feed Setup failed");
	}
	return;	
}

/** @brief FeedRequestHandler
*/
static void FeedRequestHandler(void *usrptr, int status, char *data, int datasize) {
	
	feedrequest *ptr = (feedrequest *)usrptr;
	mrss_t *mrss;
	mrss_error_t ret;
	SET_SEGV_LOCATION();


	if (status != NS_SUCCESS) {
		dlog(DEBUG1, "RSS Scrape Download failed: %s", data);
		irc_prefmsg(feed_bot, ptr->c, "RSS Scrape Failed. %s not added", ptr->fi->feedurl);
		ns_free(ptr->fi);
		ns_free(ptr);
		return;
	}
	mrss = NULL;
	mrss_new(&mrss);
	/* ok, here is our data */
	ret = mrss_parse_buffer(data, datasize, &mrss);
	if (ret != MRSS_OK) {
		dlog(DEBUG1, "RSS Parse Failed: %s", mrss_strerror(ret));
		irc_prefmsg(feed_bot, ptr->c, "RSS Parse Failed for URL %s: %s", ptr->fi->feedurl, mrss_strerror(ret));
		mrss_free(mrss);
		ns_free(ptr->fi);
		ns_free(ptr);
		return;
	}
	strncpy(ptr->fi->title, mrss->title, BUFSIZE);
	if (mrss->description)
		strncpy(ptr->fi->description, mrss->description, BUFSIZE);
	if (mrss->link)
		strncpy(ptr->fi->link, mrss->link, BUFSIZE);
	if (mrss->about) 
		strncpy(ptr->fi->about, mrss->about, BUFSIZE);
	ptr->fi->setup = 1;
	mrss_free(mrss);
	lnode_create_append(lofeeds, ptr->fi);
	/* save it to our DB */
	DBAStore("feeds", ptr->fi->feedurl, (void *)ptr->fi, sizeof(feedinfo));
	irc_prefmsg(feed_bot, ptr->c, "%s has been added to Feed List", ptr->fi->title);
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
	FILE *feeds;
	static char buf[BUFSIZE];
	feedinfo *fi, *ff = NULL;
	lnode_t *node;
	SET_SEGV_LOCATION();
	lofeeds = list_create( LISTCOUNT_T_MAX );
	subscribedfeeds = hash_create (HASHCOUNT_T_MAX, NULL, NULL);
	/* XXX hard coded? */
	feed.interval = 60;
	DBAFetchRows2("feeds", load_feeds);
	/* if lofeeds count is 0, this is probably a first time load, so load from text file */
	if (list_count(lofeeds) <= 0)  {
		feeds = os_fopen("data/feeds.dat", "r");
		if (!feeds) {
			nlog(LOG_WARNING, "Can't Open Feed Listing file feeds.dat. FeedServ will start with no list of feeds");
		} else {
			while (os_fgets(buf, BUFSIZE, feeds)) {
				fi = os_malloc(sizeof(feedinfo));
				strncpy(fi->feedurl, trim(buf), BUFSIZE);
				fi->setup = 0;
				lnode_create_append(lofeeds, fi);	
			}
			os_fclose(feeds);
			node = list_first(lofeeds);
			if (node) ff = lnode_get(node);
			if ((ff) && ((new_transfer(ff->feedurl, NULL, NS_MEMORY, "", node, FeedSetupHandler) != NS_SUCCESS ))) {
					nlog(LOG_WARNING, "Download Feed Setup failed");
			}
		}
	}
						
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

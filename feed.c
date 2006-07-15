
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
	char *title;
	char *description;
	char *link;
	char *about;
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
} subscribed;

typedef struct feedrequest {
	Client *c;
	feedinfo *fi;
} feedrequest;

list_t *lofeeds;
list_t *subscribedfeeds;


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
	return;
	ptr = ns_malloc(sizeof(feeddata));
	ptr->mrss = NULL;
	mrss_new(&ptr->mrss);
	ptr->channel = ns_malloc(MAXCHANLEN);
	strncpy(ptr->channel, "#neostats", MAXCHANLEN);
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
  mrss_item_t *item;
  mrss_hour_t *hour;
  mrss_day_t *day;
  mrss_category_t *category;

  irc_chanprivmsg(feed_bot, ptr->channel,"Generic:");
  irc_chanprivmsg(feed_bot, ptr->channel,"file: %s", ptr->mrss->file);
  irc_chanprivmsg(feed_bot, ptr->channel,"encoding: %s", ptr->mrss->encoding);
  irc_chanprivmsg(feed_bot, ptr->channel,"size: %d", (int)ptr->mrss->size);

  irc_chanprivmsg(feed_bot, ptr->channel,"version:");
  switch (ptr->mrss->version)
    {
    case MRSS_VERSION_0_91:
      irc_chanprivmsg(feed_bot, ptr->channel," 0.91");
      break;

    case MRSS_VERSION_0_92:
      irc_chanprivmsg(feed_bot, ptr->channel," 0.92");
      break;

    case MRSS_VERSION_1_0:
      irc_chanprivmsg(feed_bot, ptr->channel," 1.0");
      break;

    case MRSS_VERSION_2_0:
      irc_chanprivmsg(feed_bot, ptr->channel," 2.0");
      break;
    }

  irc_chanprivmsg(feed_bot, ptr->channel,"Channel:");
  irc_chanprivmsg(feed_bot, ptr->channel,"title: %s", ptr->mrss->title);
  irc_chanprivmsg(feed_bot, ptr->channel,"description: %s", ptr->mrss->description);
  irc_chanprivmsg(feed_bot, ptr->channel,"link: %s", ptr->mrss->link);
  irc_chanprivmsg(feed_bot, ptr->channel,"language: %s", ptr->mrss->language);
  irc_chanprivmsg(feed_bot, ptr->channel,"rating: %s", ptr->mrss->rating);
  irc_chanprivmsg(feed_bot, ptr->channel,"copyright: %s", ptr->mrss->copyright);
  irc_chanprivmsg(feed_bot, ptr->channel,"pubDate: %s", ptr->mrss->pubDate);
  irc_chanprivmsg(feed_bot, ptr->channel,"lastBuildDate: %s", ptr->mrss->lastBuildDate);
  irc_chanprivmsg(feed_bot, ptr->channel,"docs: %s", ptr->mrss->docs);
  irc_chanprivmsg(feed_bot, ptr->channel,"managingeditor: %s", ptr->mrss->managingeditor);
  irc_chanprivmsg(feed_bot, ptr->channel,"webMaster: %s", ptr->mrss->webMaster);
  irc_chanprivmsg(feed_bot, ptr->channel,"generator: %s", ptr->mrss->generator);
  irc_chanprivmsg(feed_bot, ptr->channel,"ttl: %d", ptr->mrss->ttl);
  irc_chanprivmsg(feed_bot, ptr->channel,"about: %s", ptr->mrss->about);

  irc_chanprivmsg(feed_bot, ptr->channel,"Image:");
  irc_chanprivmsg(feed_bot, ptr->channel,"image_title: %s", ptr->mrss->image_title);
  irc_chanprivmsg(feed_bot, ptr->channel,"image_url: %s", ptr->mrss->image_url);
  irc_chanprivmsg(feed_bot, ptr->channel,"image_link: %s", ptr->mrss->image_link);
  irc_chanprivmsg(feed_bot, ptr->channel,"image_width: %d", ptr->mrss->image_width);
  irc_chanprivmsg(feed_bot, ptr->channel,"image_height: %d", ptr->mrss->image_height);
  irc_chanprivmsg(feed_bot, ptr->channel,"image_description: %s", ptr->mrss->image_description);

  irc_chanprivmsg(feed_bot, ptr->channel,"TextInput:");
  irc_chanprivmsg(feed_bot, ptr->channel,"textinput_title: %s", ptr->mrss->textinput_title);
  irc_chanprivmsg(feed_bot, ptr->channel,"textinput_description: %s",
	   ptr->mrss->textinput_description);
  irc_chanprivmsg(feed_bot, ptr->channel,"textinput_name: %s", ptr->mrss->textinput_name);
  irc_chanprivmsg(feed_bot, ptr->channel,"textinput_link: %s", ptr->mrss->textinput_link);

  irc_chanprivmsg(feed_bot, ptr->channel,"Cloud:");
  irc_chanprivmsg(feed_bot, ptr->channel,"cloud: %s", ptr->mrss->cloud);
  irc_chanprivmsg(feed_bot, ptr->channel,"cloud_domain: %s", ptr->mrss->cloud_domain);
  irc_chanprivmsg(feed_bot, ptr->channel,"cloud_port: %d", ptr->mrss->cloud_port);
  irc_chanprivmsg(feed_bot, ptr->channel,"cloud_registerProcedure: %s",
	   ptr->mrss->cloud_registerProcedure);
  irc_chanprivmsg(feed_bot, ptr->channel,"cloud_protocol: %s", ptr->mrss->cloud_protocol);

  irc_chanprivmsg(feed_bot, ptr->channel,"SkipHours:");
  hour = ptr->mrss->skipHours;
  while (hour)
    {
      irc_chanprivmsg(feed_bot, ptr->channel,"%s", hour->hour);
      hour = hour->next;
    }

  irc_chanprivmsg(feed_bot, ptr->channel,"SkipDays:");
  day = ptr->mrss->skipDays;
  while (day)
    {
      irc_chanprivmsg(feed_bot, ptr->channel,"%s", day->day);
      day = day->next;
    }

  irc_chanprivmsg(feed_bot, ptr->channel,"Category:");
  category = ptr->mrss->category;
  while (category)
    {
      irc_chanprivmsg(feed_bot, ptr->channel,"category: %s", category->category);
      irc_chanprivmsg(feed_bot, ptr->channel,"category_domain: %s", category->domain);
      category = category->next;
    }

  irc_chanprivmsg(feed_bot, ptr->channel,"Items:");
  item = ptr->mrss->item;
  while (item)
    {
      irc_chanprivmsg(feed_bot, ptr->channel,"title: %s", item->title);
      irc_chanprivmsg(feed_bot, ptr->channel,"link: %s", item->link);
      irc_chanprivmsg(feed_bot, ptr->channel,"description: %s", html_trim(item->description));
      irc_chanprivmsg(feed_bot, ptr->channel,"author: %s", item->author);
      irc_chanprivmsg(feed_bot, ptr->channel,"comments: %s", item->comments);
      irc_chanprivmsg(feed_bot, ptr->channel,"pubDate: %s", item->pubDate);
      irc_chanprivmsg(feed_bot, ptr->channel,"guid: %s", item->guid);
      irc_chanprivmsg(feed_bot, ptr->channel,"guid_isPermaLink: %d", item->guid_isPermaLink);
      irc_chanprivmsg(feed_bot, ptr->channel,"source: %s", item->source);
      irc_chanprivmsg(feed_bot, ptr->channel,"source_url: %s", item->source_url);
      irc_chanprivmsg(feed_bot, ptr->channel,"enclosure: %s", item->enclosure);
      irc_chanprivmsg(feed_bot, ptr->channel,"enclosure_url: %s", item->enclosure_url);
      irc_chanprivmsg(feed_bot, ptr->channel,"enclosure_length: %d", item->enclosure_length);
      irc_chanprivmsg(feed_bot, ptr->channel,"enclosure_type: %s", item->enclosure_type);

      irc_chanprivmsg(feed_bot, ptr->channel,"Category:");
      category = item->category;
      while (category)
	{
	  irc_chanprivmsg(feed_bot, ptr->channel,"category: %s", category->category);
	  irc_chanprivmsg(feed_bot, ptr->channel,"category_domain: %s", category->domain);
	  category = category->next;
	}

      item = item->next;
    }

  mrss_free (ptr->mrss);

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

static int load_feeds( void *data, int size )
{
	feedinfo *fi;	
	if( size == sizeof(feedinfo) )
	{
		fi = ns_calloc( sizeof(feedinfo));
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
	ptr->title = strndup(mrss->title, strlen(mrss->title));
	if (mrss->description)
		ptr->description = strndup(mrss->description, strlen(mrss->description));
	if (mrss->link)
		ptr->link = strndup( mrss->link, strlen(mrss->link));
	if (mrss->about) {
		ptr->about = strndup(mrss->about, strlen(mrss->about));
	} else {
		ptr->about = NULL;
	}
	ptr->setup = 1;
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
	ptr->fi->title = strndup(mrss->title, strlen(mrss->title));
	ptr->fi->description = strndup(mrss->description, strlen(mrss->description));
	ptr->fi->link = strndup(mrss->link, strlen(mrss->link));
	if (mrss->about) {
		ptr->fi->about = strndup(mrss->about, strlen(mrss->about));
	} else {
		ptr->fi->about = NULL;
	}
	ptr->fi->setup = 1;
	mrss_free(mrss);
	lnode_create_append(lofeeds, ptr->fi);
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
	subscribedfeeds = list_create (LISTCOUNT_T_MAX );
	DBAFetchRows("feeds", load_feeds);
	/* if lofeeds count is 0, this is probably a first time load, so load from text file */
	if (list_count(lofeeds) <= 0)  {
		feeds = os_fopen("data/feeds.dat", "r");
		if (!feeds) {
			nlog(LOG_WARNING, "Can't Open Feed Listing file feeds.dat. FeedServ will start with no list of feeds");
		} else {
			while (os_fgets(buf, BUFSIZE, feeds)) {
				fi = os_malloc(sizeof(feedinfo));
				strncpy(fi->feedurl, buf, BUFSIZE);
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

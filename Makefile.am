AUTOMAKE_OPTIONS        = foreign
ACLOCAL_AMFLAGS         = -I autotools
AM_MAKEFLAGS            = -s

pkglib_LTLIBRARIES	= feedserv.la
feedserv_la_SOURCES	= feed.c  feed_help.c
noinst_HEADERS		= feed.h 

feedserv_la_LDFLAGS	= -module -avoid-version


EXTRA_DIST              	= autotools/ccdv.c autotools/shtool RELNOTES
dist_doc_DATA		= README.feedserv README.feedserv.html

include $(top_srcdir)/autotools/rules.mk

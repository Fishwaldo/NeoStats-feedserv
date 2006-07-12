AUTOMAKE_OPTIONS        = foreign
ACLOCAL_AMFLAGS         = -I autotools
AM_MAKEFLAGS            = -s

pkglib_LTLIBRARIES	= libfeedserv.la
libfeedserv_la_SOURCES	= feed.c  feed_help.c
noinst_HEADERS		= feed.h 

libfeedserv_la_LDFLAGS	= -module -avoid-version


EXTRA_DIST              	= autotools/ccdv.c autotools/shtool RELNOTES
dist_doc_DATA		= README.feed README.feed.html

include $(top_srcdir)/autotools/rules.mk

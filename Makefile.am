lib_LTLIBRARIES = dodgeball.la

dodgeball_la_SOURCES = dodgeball.cpp
dodgeball_la_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
dodgeball_la_LDFLAGS = -module -avoid-version -shared
dodgeball_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.md \
	dodgeball.def \
	dodgeball.sln \
	dodgeball.vcxproj

MAINTAINERCLEANFILES =	\
	Makefile.in
